#define JE_IMPL
#include "jeecs/basic.hpp"
#include "jeecs/game.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/math.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs_core_graphic_system.hpp"
#include "jeecs_core_render_helpers.hpp"

#define JE_LIGHT2D_DEFER_0 4

namespace jeecs::graphic
{
    using namespace slice_requirement;

    namespace
    {
        inline const float screen_quad_data[] = {
            -1.f,
            1.f,
            0.f,
            0.f,
            1.f,
            -1.f,
            -1.f,
            0.f,
            0.f,
            0.f,
            1.f,
            1.f,
            0.f,
            1.f,
            1.f,
            1.f,
            -1.f,
            0.f,
            1.f,
            0.f,
        };
        // WallShadow（墙状遮挡）全向阴影条带：底/顶/左/右四条锚边
        // 各一个四边形，锚点对（factor=0）贴边，远端对（factor=1）由
        // 顶点着色器沿背光方向挤出；向光边在顶点着色器里折叠回锚边
        // （零面积，无片元）。顶点格式 pos.xyz / uv.xy / factor /
        // edge_normal.xy，uv 沿边方向 0→1、沿挤出方向从本边值跨到
        // 对边值（整图压扁观感）。
        inline const float wall_shadow_strip_data[] = {
            /* bottom edge, normal (0,-1) */
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
            0.5f,  -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f,
            -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f,
            0.5f,  -0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, -1.0f,
            /* top edge, normal (0,1) */
            -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            -0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
            0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
            /* left edge, normal (-1,0) */
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f,
            -0.5f, 0.5f,  0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f,
            -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 0.0f,
            -0.5f, 0.5f,  0.0f, 1.0f, 1.0f, 1.0f, -1.0f, 0.0f,
            /* right edge, normal (1,0) */
            0.5f,  -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            0.5f,  0.5f,  0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
            0.5f,  -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
            0.5f,  0.5f,  0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f,
        };
        // SpriteShadow 单位四边形 [-1,1]²，仅位置属性
        inline const float sprite_shadow_quad_data[] = {
            -1.f, -1.f, 0.f,
            1.f, -1.f, 0.f,
            -1.f, 1.f, 0.f,
            1.f, 1.f, 0.f,
        };

        // 自阴影的 painter key 前移量（世界单位）：同一图层的 BlockShadow
        // 写实体/图层 key，自阴影写在它之前半层，于是按 painter key 的
        // “最靠前胜出”规则清除本层遮挡落在本层表面上的自遮挡。
        constexpr float SELF_SHADOW_SURFACE_BIAS = 0.005f;

        // 引擎内置着色器：加载失败即显式失败（核心管线资源约定）。
        basic::resource<graphic::shader> load_builtin_shader(const char* path)
        {
            return graphic::shader::load(nullptr, path).value();
        }

        // 按比例缩放的缓冲尺寸：比例钳制到 [0, 1]，结果至少为 1。
        size_t scaled_buffer_size(size_t base, float ratio)
        {
            return std::max((size_t)1, (size_t)llround(
                base * std::max(0.f, std::min(ratio, 1.0f))));
        }
    }

    DeferLight2DGraphicPipelineSystem::DeferLight2DResources::DeferLight2DResources()
        : no_shadow{
            graphic::texture2d::create(1, 1, JE_GL_TEXTURE_FORMAT_RGBA)
        }
        , screen_quad{
            graphic::vertex::create(
                JE_GL_VERTEX_TYPE_TRIANGLESTRIP,
                screen_quad_data,
                sizeof(screen_quad_data),
                {0, 1, 2, 3},
                {
                    {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3},
                    {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 2},
                })
                .value()
        }
        , wall_shadow_strips{
            graphic::vertex::create(
                JE_GL_VERTEX_TYPE_TRIANGLES,
                wall_shadow_strip_data,
                sizeof(wall_shadow_strip_data),
                {0, 1, 2, 1, 3, 2,
                4, 5, 6, 5, 7, 6,
                8, 9, 10, 9, 11, 10,
                12, 13, 14, 13, 15, 14},
                {
                    {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3},
                    {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 2},
                    {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 1},
                    {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 2},
                })
                .value()
        }
        , sprite_shadow_quad{
            graphic::vertex::create(
                JE_GL_VERTEX_TYPE_TRIANGLESTRIP,
                sprite_shadow_quad_data,
                sizeof(sprite_shadow_quad_data),
                {0, 1, 2, 3},
                {
                    {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3},
                })
                .value()
        }
        , point_shadow{
            load_builtin_shader("!/builtin/shader/internal/defer_light2d_shadow_point.shader"),
            load_builtin_shader("!/builtin/shader/internal/defer_light2d_shadow_reverse_point.shader"),
            load_builtin_shader("!/builtin/shader/internal/defer_light2d_shadow_point_shape.shader"),
            load_builtin_shader("!/builtin/shader/internal/defer_light2d_shadow_point_sprite.shader"),
            load_builtin_shader("!/builtin/shader/internal/defer_light2d_shadow_point_wall.shader"),
        }
        , parallel_shadow{
            load_builtin_shader("!/builtin/shader/internal/defer_light2d_shadow_parallel.shader"),
            load_builtin_shader("!/builtin/shader/internal/defer_light2d_shadow_reverse_parallel.shader"),
            load_builtin_shader("!/builtin/shader/internal/defer_light2d_shadow_parallel_shape.shader"),
            load_builtin_shader("!/builtin/shader/internal/defer_light2d_shadow_parallel_sprite.shader"),
            load_builtin_shader("!/builtin/shader/internal/defer_light2d_shadow_parallel_wall.shader"),
        }
        , self_shadow{
            load_builtin_shader("!/builtin/shader/internal/defer_light2d_shadow_sub.shader")
        }
        , self_shadow_array{
            load_builtin_shader("!/builtin/shader/internal/defer_light2d_shadow_sub_array.shader")
        }
        , self_shadow_topdown{
            load_builtin_shader("!/builtin/shader/internal/defer_light2d_shadow_sub_topdown.shader")
        }
        , self_shadow_topdown_array{
            load_builtin_shader("!/builtin/shader/internal/defer_light2d_shadow_sub_topdown_array.shader")
        }
    {
    }

    DeferLight2DGraphicPipelineSystem::DeferLight2DGraphicPipelineSystem(game_world w)
        : GraphicPipelineBase(w)
    {
    }

    DeferLight2DGraphicPipelineSystem::~DeferLight2DGraphicPipelineSystem()
    {
    }

    void DeferLight2DGraphicPipelineSystem::GraphicUpdate()
    {
        begin_frame();

        m_lights.clear();
        m_blocks.clear();
        m_cameras.clear();
        m_renderers.clear();

        for (auto&& [
            tarns,
            projection,
            rendqueue,
            cameraviewport,
            rendbuf,
            light2dpostpass,
            shaders,
            textures,
            frustumCulling,
            clear,
            color] :
        query<
            view typesof(
                Translation&,
                Projection&,
                Rendqueue*,
                Viewport*,
                RendToFramebuffer*,
                CameraPostPass*,
                Shaders*,
                Textures2D*,
                FrustumCulling*,
                Clear*,
                Color*
            )
        >())
        {
            auto* branch = this->allocate_branch(rendqueue == nullptr ? 0 : rendqueue->rend_queue);
            m_cameras.push_back(
                l2dcamera_arch{
                    camera_arch{
                        branch,
                        rendqueue,
                        &projection,
                        cameraviewport,
                        rendbuf,
                        frustumCulling,
                        clear
                    },
                    &tarns,
                    light2dpostpass,
                    shaders,
                    textures,
                    color
                });

            if (light2dpostpass != nullptr)
            {
                graphic::framebuffer* rend_aim_buffer = (rendbuf != nullptr && rendbuf->framebuffer.has_value())
                    ? rendbuf->framebuffer->get()
                    : nullptr;

                const size_t RENDAIMBUFFER_WIDTH =
                    std::max((size_t)1, (size_t)llround(
                        (cameraviewport ? cameraviewport->viewport.z : 1.0f) *
                        (rend_aim_buffer ? rend_aim_buffer->width() : m_window_width)));
                const size_t RENDAIMBUFFER_HEIGHT =
                    std::max((size_t)1, (size_t)llround(
                        (cameraviewport ? cameraviewport->viewport.w : 1.0f) *
                        (rend_aim_buffer ? rend_aim_buffer->height() : m_window_height)));

                const size_t LIGHT_BUFFER_WIDTH =
                    scaled_buffer_size(RENDAIMBUFFER_WIDTH, light2dpostpass->light_rend_ratio);
                const size_t LIGHT_BUFFER_HEIGHT =
                    scaled_buffer_size(RENDAIMBUFFER_HEIGHT, light2dpostpass->light_rend_ratio);

                bool need_update = !light2dpostpass->post_rend_target.has_value();

                if (!need_update)
                {
                    auto& light_target = light2dpostpass->post_light_target.value();
                    auto& rend_target = light2dpostpass->post_rend_target.value();

                    if (light_target->width() != LIGHT_BUFFER_WIDTH
                        || light_target->height() != LIGHT_BUFFER_HEIGHT
                        || rend_target->width() != RENDAIMBUFFER_WIDTH
                        || rend_target->height() != RENDAIMBUFFER_HEIGHT)
                        need_update = true;
                }
                if (need_update && RENDAIMBUFFER_WIDTH > 0 && RENDAIMBUFFER_HEIGHT > 0)
                {
                    light2dpostpass->post_rend_target
                        = jeecs::graphic::framebuffer::create(
                            RENDAIMBUFFER_WIDTH,
                            RENDAIMBUFFER_HEIGHT,
                            {
                                // 漫反射颜色
                                JE_GL_TEXTURE_FORMAT_RGBA,
                                // 自发光颜色，用于法线反射或者发光物体的颜色参数，最终混合shader会将此参数用于光照计算
                                je_gl_texture_format_t(JE_GL_TEXTURE_FORMAT_RGBA | JE_GL_TEXTURE_FORMAT_FLOAT16),
                                // 视空间坐标(RGB) Alpha通道暂时留空
                                je_gl_texture_format_t(JE_GL_TEXTURE_FORMAT_RGBA | JE_GL_TEXTURE_FORMAT_FLOAT16),
                                // 法线空间颜色
                                je_gl_texture_format_t(JE_GL_TEXTURE_FORMAT_RGBA | JE_GL_TEXTURE_FORMAT_FLOAT16),
                            },
                            true).value();
                    light2dpostpass->post_light_target
                        = jeecs::graphic::framebuffer::create(
                            LIGHT_BUFFER_WIDTH,
                            LIGHT_BUFFER_HEIGHT,
                            {
                                // 光渲染结果
                                (je_gl_texture_format_t)(JE_GL_TEXTURE_FORMAT_RGBA | JE_GL_TEXTURE_FORMAT_FLOAT16),
                            },
                            false).value();
                }
            }
        }

        sort_by_rend_queue(m_cameras);

        for (auto&& [trans, shads, texs, texarr, shape, rendqueue, color] : query<
            view typesof(
                Translation&,
                Shaders&,
                Textures2D*,
                Textures2DArray*,
                Shape&,
                Rendqueue*,
                Color*
            ),
            except typesof(
                Point,
                Parallel,
                Range,
                Element
            )
        >())
        {
            m_renderers.push_back(
                world_renderer_arch{
                    color, rendqueue, &trans, &shape, &shads, texs, texarr
                });
        }
        sort_by_rend_queue(m_renderers);

        for (auto&& [
            trans,
            topdown,
            point,
            parallel,
            range,
            shadowoffset,
            gain,
            shadowbuffer,
            color,
            shape,
            shads,
            texs
        ] :
        query<
            view typesof(
                Translation&,
                TopDown*,
                Point*,
                Parallel*,
                Range*,
                ShadowOffset*,
                Gain*,
                ShadowBuffer*,
                Color*,
                Shape&,
                Shaders&,
                Textures2D*
            ),
            anyof typesof(
                Point,
                Parallel,
                Range
            )
        >())
        {
            m_lights.emplace_back(
                light2d_arch{
                    &trans, topdown, point, parallel, range, shadowoffset, gain, shadowbuffer,
                    color, &shape, &shads, texs,
                });
            if (shadowbuffer != nullptr)
            {
                const size_t SHADOW_BUFFER_WIDTH =
                    scaled_buffer_size(m_window_width, shadowbuffer->resolution_ratio);
                const size_t SHADOW_BUFFER_HEIGHT =
                    scaled_buffer_size(m_window_height, shadowbuffer->resolution_ratio);

                bool generate_new_framebuffer = !shadowbuffer->buffer.has_value();
                if (!generate_new_framebuffer)
                {
                    auto& buffer = shadowbuffer->buffer.value();
                    if (buffer->width() != SHADOW_BUFFER_WIDTH
                        || buffer->height() != SHADOW_BUFFER_HEIGHT)
                        generate_new_framebuffer = true;
                }

                if (generate_new_framebuffer)
                {
                    shadowbuffer->buffer = graphic::framebuffer::create(
                        SHADOW_BUFFER_WIDTH,
                        SHADOW_BUFFER_HEIGHT,
                        {
                            JE_GL_TEXTURE_FORMAT_RGBA,
                            // R = 世界/遮挡阴影强度（MAX 合并）
                        },
                        // 深度附件：DeferLight2D 的逐像素排序键。
                        // 深度值不是相机深度，而是 painter key（TopDown
                        // 世界 Y / 普通世界 Z）经每光源线性映射后的值；
                        // ZTEST(LESS_EQUAL) 保留最靠前的阴影贡献。
                        true).value();
                }
            }
            if (range != nullptr)
            {
                if (!range->shape.m_light_mesh.has_value()
                    && range->shape.m_point_count != 0
                    && !range->shape.m_positions.empty())
                {
                    std::vector<float> vertex_datas;
                    std::vector<uint32_t> index_datas;
                    auto append_point =
                        [&vertex_datas, &index_datas](const math::vec2& p, float strength)
                        {
                            vertex_datas.push_back(p.x);
                            vertex_datas.push_back(p.y);
                            vertex_datas.push_back(0.f);
                            vertex_datas.push_back(strength);

                            index_datas.push_back((uint32_t)index_datas.size());
                        };

                    size_t layer_count = range->shape.m_strength.size();
                    size_t last_point_index = 0;
                    for (size_t ilayer = 0; ilayer < layer_count; ++ilayer)
                    {
                        // 如果是第一层，那么特殊处理，
                        if (ilayer == 0)
                        {
                            for (size_t ipoint = 0; ipoint < range->shape.m_point_count; ++ipoint)
                            {
                                if (ipoint == 0)
                                    last_point_index = 0;
                                else if (ipoint % 2 == 1)
                                    last_point_index = 1 + ipoint / 2;
                                else
                                    last_point_index = range->shape.m_point_count - ipoint / 2;

                                append_point(
                                    range->shape.m_positions.at(last_point_index),
                                    range->shape.m_strength.at(0));
                            }
                        }
                        else
                        {
                            // 如果是点的数量是偶数，那么接下来的层的点需要反向旋转
                            for (size_t tipoint = 0; tipoint < range->shape.m_point_count; ++tipoint)
                            {
                                size_t real_ipoint;
                                size_t real_next_last_layer_ipoint;
                                if (range->shape.m_point_count % 2 == 0)
                                {
                                    // 反向旋转
                                    real_ipoint = (last_point_index + range->shape.m_point_count - tipoint) % range->shape.m_point_count;
                                    real_next_last_layer_ipoint = (last_point_index + range->shape.m_point_count - tipoint - 1) % range->shape.m_point_count;
                                }
                                else
                                {
                                    // 正向旋转
                                    real_ipoint = (last_point_index + tipoint) % range->shape.m_point_count;
                                    real_next_last_layer_ipoint = (last_point_index + tipoint + 1) % range->shape.m_point_count;
                                }

                                append_point(
                                    range->shape.m_positions[real_ipoint + ilayer * range->shape.m_point_count],
                                    range->shape.m_strength.at(ilayer));

                                // 如果不是最后一个点，那么链接到下一个顺位点
                                if (tipoint + 1 != range->shape.m_point_count)
                                    append_point(
                                        range->shape.m_positions[real_next_last_layer_ipoint + (ilayer - 1) * range->shape.m_point_count],
                                        range->shape.m_strength.at(ilayer - 1));
                            }

                            // 最后链接到本层的第一个顺位点
                            append_point(
                                range->shape.m_positions[last_point_index + ilayer * range->shape.m_point_count],
                                range->shape.m_strength.at(ilayer));
                        }
                    }

                    range->shape.m_light_mesh = jeecs::graphic::vertex::create(
                        JE_GL_VERTEX_TYPE_TRIANGLESTRIP,
                        vertex_datas.data(), vertex_datas.size() * sizeof(float),
                        index_datas,
                        {
                            {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3},
                            {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 1},
                        });
                }
            }
        }

        for (auto&& [trans, blockshadow, shapeshadow, spriteshadow, wallshadow, selfshadow,
            perblock, texture, texture_array, shape] : query<
            view typesof(
                Translation&,
                BlockShadow*,
                ShapeShadow*,
                SpriteShadow*,
                WallShadow*,
                SelfShadow*,
                PerBlockSelfShadow*,
                Textures2D*,
                Textures2DArray*,
                Shape&
            ),
            anyof typesof(
                BlockShadow,
                ShapeShadow,
                SpriteShadow,
                WallShadow,
                SelfShadow
            )
        >())
        {
            if (blockshadow != nullptr)
            {
                if (!blockshadow->shape.m_block_mesh.has_value())
                {
                    std::vector<float> _vertex_buffer;
                    std::vector<uint32_t> _index_buffer;
                    if (!blockshadow->shape.m_block_points.empty())
                    {
                        // 通用 block 的逐段 key 使用多边形最低局部 Y；
                        // 瓦片分片走 write_part_blockshadow_mesh 的逐段
                        // key。vertex_priority=false 时 shader 会用实体
                        // 底边 key 覆盖它，这里只保证顶点布局一致。
                        float _min_point_y = blockshadow->shape.m_block_points.front().y;
                        for (auto& point : blockshadow->shape.m_block_points)
                            _min_point_y = std::min(_min_point_y, point.y);

                        for (auto& point : blockshadow->shape.m_block_points)
                        {
                            _vertex_buffer.insert(_vertex_buffer.end(),
                                {
                                    point.x, point.y, 0.f, 0.f, _min_point_y,
                                    point.x, point.y, 0.f, 1.f, _min_point_y,
                                });

                            uint32_t _index_offset = _index_buffer.size();
                            _index_buffer.insert(_index_buffer.end(),
                                {
                                    _index_offset,
                                    _index_offset + 1,
                                });
                        }
                        blockshadow->shape.m_block_mesh = jeecs::graphic::vertex::create(
                            JE_GL_VERTEX_TYPE_TRIANGLESTRIP,
                            _vertex_buffer.data(), _vertex_buffer.size() * sizeof(float),
                            _index_buffer,
                            {
                                {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3},
                                {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 1},
                                {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 1},
                            });
                    }
                    else
                        blockshadow->shape.m_block_mesh.reset();
                }
            }
            math::vec3 top_world_center;
            m_blocks.push_back(
                block2d_arch{
                    &trans,
                    get_block_bottom_world_center(trans, shape, &top_world_center),
                    top_world_center,
                    blockshadow,
                    shapeshadow,
                    spriteshadow,
                    wallshadow,
                    selfshadow,
                    perblock,
                    texture,
                    texture_array,
                    &shape
                }
            );
        }

        end_frame();
        DrawFrame();
    }

    const DeferLight2DGraphicPipelineSystem::shadow_pass_set& DeferLight2DGraphicPipelineSystem::shadow_passes_for(bool is_parallel) const
    {
        return is_parallel ? m_resources.parallel_shadow : m_resources.point_shadow;
    }

    void DeferLight2DGraphicPipelineSystem::bind_shadow_texture(
        je_gl_command_buffer_t* chain, /* OPTIONAL */ const Textures2D* textures)
    {
        auto main_texture =
            textures == nullptr ? std::nullopt : textures->get_texture(0);

        je_graphic_cmd_bind_texture_2d(
            chain, 0,
            main_texture.has_value()
            ? main_texture.value()->resource()
            : m_defaults.default_texture->resource());
    }

    math::vec3 DeferLight2DGraphicPipelineSystem::get_block_bottom_world_center(
        const Translation& translation, const Shape& shape,
        /* OPTIONAL */ math::vec3* top_out) const
    {
        const auto* vertex_data = m_defaults.shape_of(shape.vertex)->resource();
        const float(&MAT4_MODEL)[4][4] = translation.object2world;

        math::vec3 min_point;
        math::vec3 max_point;
        for (int corner = 0; corner < 8; ++corner)
        {
            const float local_point[4] = {
                (corner & 1) ? vertex_data->m_x_max : vertex_data->m_x_min,
                (corner & 2) ? vertex_data->m_y_max : vertex_data->m_y_min,
                (corner & 4) ? vertex_data->m_z_max : vertex_data->m_z_min,
                1.f };
            float world_point[4];
            math::mat4xvec4(world_point, MAT4_MODEL, local_point);

            if (corner == 0)
            {
                min_point = math::vec3(world_point[0], world_point[1], world_point[2]);
                max_point = min_point;
            }
            else
            {
                min_point.x = std::min(min_point.x, world_point[0]);
                min_point.y = std::min(min_point.y, world_point[1]);
                min_point.z = std::min(min_point.z, world_point[2]);
                max_point.x = std::max(max_point.x, world_point[0]);
                max_point.y = std::max(max_point.y, world_point[1]);
                max_point.z = std::max(max_point.z, world_point[2]);
            }
        }
        if (top_out != nullptr)
        {
            *top_out = math::vec3(
                (min_point.x + max_point.x) * 0.5f,
                max_point.y,
                (min_point.z + max_point.z) * 0.5f);
        }
        return math::vec3(
            (min_point.x + max_point.x) * 0.5f,
            min_point.y,
            (min_point.z + max_point.z) * 0.5f);
    }

    DeferLight2DGraphicPipelineSystem::shadow_depth_mapping DeferLight2DGraphicPipelineSystem::make_shadow_depth_mapping(
        const std::vector<block2d_arch>& blocks, bool topdown) const
    {
        shadow_depth_mapping mapping;
        if (blocks.empty())
            return mapping;

        const auto key_of_bottom = [topdown](const block2d_arch& block)
            {
                return topdown
                    ? block.bottom_world_center.y
                    : block.translation->world_position.z;
            };

        float key_min = key_of_bottom(blocks.front());
        float key_max = topdown
            ? blocks.front().top_world_center.y
            : blocks.front().translation->world_position.z;

        for (const auto& block : blocks)
        {
            key_min = std::min(key_min, key_of_bottom(block));
            key_max = std::max(key_max, topdown
                ? block.top_world_center.y
                : block.translation->world_position.z);
        }

        const float margin = 1.f;
        const float range = std::max(key_max - key_min, 1.f) + 2.f * margin;
        mapping.scale = 0.9f / range;
        mapping.bias = 0.05f - (key_min - margin) * mapping.scale;
        return mapping;
    }

    void DeferLight2DGraphicPipelineSystem::draw_shadow_occluder(
        je_gl_command_buffer_t* shadow_chain,
        const basic::resource<graphic::shader>& shadow_shader,
        const basic::resource<graphic::vertex>& shape_vertex,
        const Translation* occluder_translation,
        const light2d_arch& light,
        const math::vec3& shadow_cast_pos,
        const float(&view)[4][4],
        const float(&vp)[4][4],
        math::vec3 local_scale,
        float color_w,
        const math::vec3& parallel_dir_ref,
        const Textures2D* textures,
        const math::vec4& shadow_key,
        /* OPTIONAL */ const math::vec4* color_override,
        /* OPTIONAL */ const math::vec4* ellipse_params)
    {
        auto* vertex_resource = shape_vertex->resource();
        auto* pipeline = pipeline_for(
            shadow_shader.get(), vertex_resource);
        je_graphic_cmd_bind_pipeline(shadow_chain, pipeline);
        je_graphic_cmd_bind_vertex(shadow_chain, vertex_resource);
        render_builtin_uniforms uniforms(shadow_chain, pipeline);

        const float(&MAT4_MODEL)[4][4] = occluder_translation->object2world;
        float MAT4_MVP[4][4], MAT4_MV[4][4];
        math::mat4xmat4(MAT4_MVP, vp, MAT4_MODEL);
        math::mat4xmat4(MAT4_MV, view, MAT4_MODEL);

        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_model, float4x4, MAT4_MODEL);
        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_model_view, float4x4, MAT4_MV);
        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_model_view_projection, float4x4, MAT4_MVP);
        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_local_scale, float3,
            local_scale.x, local_scale.y, local_scale.z);
        // 阴影逐像素排序键：x=实体/顶点 key，y/z=深度映射，w=模式位
        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_shadow_key, float4,
            shadow_key.x, shadow_key.y, shadow_key.z, shadow_key.w);

        if (textures != nullptr)
        {
            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_uv_tiling, float2,
                textures->tiling.x, textures->tiling.y);
            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_uv_offset, float2,
                textures->offset.x, textures->offset.y);
        }

        // 通过 je_color 变量传递阴影计算点（点光，含 ShadowOffset 偏移）
        // 或方向（平行光）；SpriteShadow 用 color_override 改为传脚底
        // 椭圆中心（世界空间 XY + 中心世界 Z 在 .w）
        if (color_override != nullptr)
        {
            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_color, float4,
                color_override->x,
                color_override->y,
                color_override->z,
                color_override->w);
        }
        else if (light.parallel != nullptr)
        {
            math::vec3 rotated_light_dir = light.translation->world_rotation * parallel_dir_ref;
            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_color, float4,
                rotated_light_dir.x, rotated_light_dir.y, rotated_light_dir.z, color_w);
        }
        else
        {
            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_color, float4,
                shadow_cast_pos.x,
                shadow_cast_pos.y,
                shadow_cast_pos.z,
                color_w);
        }

        // SpriteShadow 接触椭圆参数（其余遮挡物不使用）
        if (ellipse_params != nullptr)
        {
            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_shadow_ellipse, float4,
                ellipse_params->x,
                ellipse_params->y,
                ellipse_params->z,
                ellipse_params->w);
        }

        je_graphic_cmd_draw_indexed(
            shadow_chain, (uint32_t)vertex_resource->m_index_count, 0, 1);
    }

    void DeferLight2DGraphicPipelineSystem::draw_self_shadow(
        je_gl_command_buffer_t* shadow_chain,
        const block2d_arch& block,
        bool topdown_light,
        const shadow_depth_mapping& depth_mapping,
        const math::vec3& shadow_cast_pos,
        const float(&view)[4][4],
        const float(&vp)[4][4],
        float shadow_key_mode)
    {
        assert(block.selfshadow != nullptr);

        // 瓦片地图分片的图集绑在数组纹理槽 0（顶点 uv.z 为数组层号），
        // 走数组采样版 sub pass；其余实体按普通 2D 纹理采样。
        // 数组绑定在 2D 绑定之后写入槽 0（后写覆盖）
        std::optional<basic::resource<graphic::texture_array>> array_atlas;
        if (block.textures_array != nullptr)
            array_atlas = block.textures_array->get_texture(0);

        bind_shadow_texture(shadow_chain, block.textures);
        if (array_atlas.has_value())
            je_graphic_cmd_bind_texture_array(shadow_chain, 0,
                array_atlas.value()->resource());

        const bool per_block = topdown_light && block.perblock != nullptr;
        auto& sub_pass_shader = per_block
            ? (array_atlas.has_value()
                ? m_resources.self_shadow_topdown_array
                : m_resources.self_shadow_topdown)
            : (array_atlas.has_value()
                ? m_resources.self_shadow_array
                : m_resources.self_shadow);

        assert(block.shape != nullptr);
        auto* vertex_resource = m_defaults.shape_of(block.shape->vertex)->resource();
        auto* pipeline = pipeline_for(
            sub_pass_shader.get(), vertex_resource);

        je_graphic_cmd_bind_pipeline(shadow_chain, pipeline);
        je_graphic_cmd_bind_vertex(shadow_chain, vertex_resource);
        render_builtin_uniforms uniforms(shadow_chain, pipeline);

        const float(&MAT4_MODEL)[4][4] = block.translation->object2world;
        float MAT4_MVP[4][4], MAT4_MV[4][4];
        math::mat4xmat4(MAT4_MVP, vp, MAT4_MODEL);
        math::mat4xmat4(MAT4_MV, view, MAT4_MODEL);

        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_model, float4x4, MAT4_MODEL);
        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_model_view, float4x4, MAT4_MV);
        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_model_view_projection, float4x4, MAT4_MVP);

        // painter key：TopDown 用实体底边世界 Y，普通 2D 用实体世界 Z；
        // 接收面再前移半层，使同层 BlockShadow（key 稍后）被清除。
        // TopDown 逐块 key 由顶点着色器用格顶（bottom + 1）生成。
        float surface_key = topdown_light
            ? block.bottom_world_center.y
            : block.translation->world_position.z;
        float key_mode = shadow_key_mode;
        if (per_block)
            key_mode = 3.f;
        else
            surface_key -= SELF_SHADOW_SURFACE_BIAS;

        JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_shadow_key, float4,
            surface_key,
            depth_mapping.scale,
            depth_mapping.bias,
            key_mode);

        if (per_block)
        {
            // 高度线打包进 JE_COLOR 供顶点着色器逐块比较：
            // AUTO 用阴影计算点世界 Y；ON（auto_disable=false）
            // 恒暗，高度线取 +1e30 使 lit 恒 0（整块写 factor）
            const float height_line = block.selfshadow->auto_disable
                ? shadow_cast_pos.y
                : 1e30f;
            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_color, float4,
                block.selfshadow->factor,
                height_line,
                0.f,
                0.f);
        }
        // AUTO 受光面写 0（接收面清除后方阴影）；TopDown 以实体底边
        //（包围盒世界 Y 最小值）判定，避免高大实体以原点高于光源时被
        // 整块点亮；普通光源仍按原点 Z。
        else if (block.selfshadow->auto_disable
            && (topdown_light
                ? block.bottom_world_center.y >= shadow_cast_pos.y
                : block.translation->world_position.z >= shadow_cast_pos.z))
            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_color, float4, 0.f, 0.f, 0.f, 0.f);
        else
            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_color, float4,
                block.selfshadow->factor,
                block.selfshadow->factor,
                block.selfshadow->factor,
                block.selfshadow->factor);

        if (block.textures != nullptr)
        {
            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_uv_tiling, float2,
                block.textures->tiling.x, block.textures->tiling.y);
            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_uv_offset, float2,
                block.textures->offset.x, block.textures->offset.y);
        }

        je_graphic_cmd_draw_indexed(
            shadow_chain, (uint32_t)vertex_resource->m_index_count, 0, 1);
    }

    void DeferLight2DGraphicPipelineSystem::draw_sprite_shadow(
        je_gl_command_buffer_t* shadow_chain,
        const shadow_pass_set& passes,
        const block2d_arch& block,
        const light2d_arch& light,
        const math::vec3& shadow_cast_pos,
        const float(&view)[4][4],
        const float(&vp)[4][4],
        const math::vec4& shadow_key)
    {
        assert(block.spriteshadow != nullptr);

        const math::vec3 shadow_dir_world_raw =
            (light.parallel != nullptr
                ? light.translation->world_rotation * math::vec3(0.f, -1.f, 0.f)
                : block.bottom_world_center
                    - shadow_cast_pos);
        // 阴影只在 XY 平面内延伸（2D 画面），Z 分量不参与
        // 方向计算，避免光源与实体的高度差稀释方向
        const float shadow_dir_world_len = std::sqrt(
            shadow_dir_world_raw.x * shadow_dir_world_raw.x
            + shadow_dir_world_raw.y * shadow_dir_world_raw.y);
        const math::vec3 shadow_dir_world =
            shadow_dir_world_len > 0.000001f
            ? math::vec3(
                shadow_dir_world_raw.x / shadow_dir_world_len,
                shadow_dir_world_raw.y / shadow_dir_world_len,
                0.f)
            : math::vec3(0.f, -1.f, 0.f);

        assert(block.shape != nullptr);
        const math::vec3 ellipse_center_world = block.bottom_world_center;

        // 点光阴影由着色器从阴影计算点向椭圆作切线锥，
        // 需要椭圆中心到阴影计算点的平面距离（JE_COLOR.z）；
        // 平行光不用，写 0。距离取世界空间椭圆中心
        // 到阴影计算点的 XY 距离。
        float light_plane_distance = 0.f;
        if (light.parallel == nullptr)
        {
            light_plane_distance = std::sqrt(
                (ellipse_center_world.x - shadow_cast_pos.x) * (ellipse_center_world.x - shadow_cast_pos.x)
                + (ellipse_center_world.y - shadow_cast_pos.y) * (ellipse_center_world.y - shadow_cast_pos.y));
        }

        const math::vec4 ellipse_center(
            ellipse_center_world.x,
            ellipse_center_world.y,
            light_plane_distance,
            ellipse_center_world.z);

        const float shadow_distance = std::max(block.spriteshadow->distance, 0.f);
        const float shadow_fade = block.spriteshadow->fade_distance > 0.f
            ? std::min(block.spriteshadow->fade_distance, shadow_distance)
            : shadow_distance;
        const math::vec4 ellipse_params(
            block.spriteshadow->ellipse_major,
            block.spriteshadow->ellipse_minor,
            shadow_distance,
            shadow_fade);

        draw_shadow_occluder(
            shadow_chain,
            passes.sprite,
            m_resources.sprite_shadow_quad,
            block.translation,
            light,
            shadow_cast_pos,
            view, vp,
            math::vec3(
                block.spriteshadow->factor,
                shadow_dir_world.x,
                shadow_dir_world.y),
            0.f,
            math::vec3(0.f, -1.f, 0.f),
            nullptr,
            shadow_key,
            &ellipse_center,
            &ellipse_params);
    }

    void DeferLight2DGraphicPipelineSystem::DrawFrame()
    {
        const math::vec4 shader_time = current_shader_time();

        // 阴影逐像素仲裁：每帧按当前 block 列表计算一次 key→深度映射。
        // TopDown 用世界 Y，普通 2D 用世界 Z；两种模式各一份。
        const shadow_depth_mapping shadow_depth_z =
            make_shadow_depth_mapping(m_blocks, false);
        const shadow_depth_mapping shadow_depth_y =
            make_shadow_depth_mapping(m_blocks, true);

        for (auto& current_camera : m_cameras)
        {
            const auto target = resolve_camera_target(current_camera);
            if (!target.has_value())
                continue;

            graphic::framebuffer* rend_aim_buffer = target->framebuffer;
            const size_t
                RENDAIMBUFFER_WIDTH = target->width,
                RENDAIMBUFFER_HEIGHT = target->height;

            const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
            const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;
            const float(&MAT4_VP)[4][4] = current_camera.projection->view_projection;

            update_default_uniform_buffer(
                *current_camera.projection, MAT4_VIEW, MAT4_PROJECTION, MAT4_VP, shader_time);

            je_gl_command_buffer_t* rend_chain = nullptr;

            std::vector<light2d_arch*> _2dlight_after_culling;
            _2dlight_after_culling.reserve(m_lights.size());

            // If current camera contain light2d-pass, prepare light shadow here.
            if (current_camera.light2DPostPass != nullptr)
            {
                if (!current_camera.light2DPostPass->post_rend_target.has_value()
                    || !current_camera.light2DPostPass->post_light_target.has_value())
                    // Not ready, skip this frame.
                    continue;

                // Walk throw all light, rend shadows to light's ShadowBuffer.
                for (auto& lightarch : m_lights)
                {
                    const float light_range = 0.5f *
                        (lightarch.range == nullptr
                            ? get_entity_size(*lightarch.translation, lightarch.shape->vertex).length()
                            : get_entity_size(*lightarch.translation, lightarch.range->shape.m_light_mesh).length());

                    if (current_camera.frustumCulling != nullptr && lightarch.parallel == nullptr)
                    {
                        if (false == current_camera.frustumCulling->test_circle(
                            lightarch.translation->world_position,
                            light_range))
                            continue;
                    }

                    _2dlight_after_culling.push_back(&lightarch);

                    if (lightarch.shadowbuffer != nullptr)
                    {
                        assert(lightarch.shadowbuffer->buffer.has_value());

                        // 阴影计算点：挂 Light2D::ShadowOffset 时为
                        // world_position + world_rotation * offset（随实体
                        // 旋转、不乘 local_scale），本光源的所有阴影
                        // 投射与高度判定均以此为原点；光照 pass 仍按
                        // 原始 world_position 渲染
                        math::vec3 shadow_cast_pos =
                            lightarch.translation->world_position;
                        if (lightarch.shadowoffset != nullptr)
                            shadow_cast_pos =
                                shadow_cast_pos
                                + lightarch.translation->world_rotation
                                    * lightarch.shadowoffset->offset;

                        auto& light2d_shadow_aim_buffer = lightarch.shadowbuffer->buffer.value();
                        je_gl_command_buffer_t* light2d_shadow_rend_chain = new_rend_chain_for_camera(
                            current_camera.branchPipeline,
                            /*viewport=*/nullptr,
                            light2d_shadow_aim_buffer.get(),
                            light2d_shadow_aim_buffer->width(),
                            light2d_shadow_aim_buffer->height());

                        const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
                        je_graphic_cmd_clear_color(light2d_shadow_rend_chain, 0, clear_color);
                        // 深度清为 1（远）；shadow shader 把 painter key 映射
                        // 到 [0.05, 0.95]，LESS_EQUAL 保留最小（最靠前）值
                        je_graphic_cmd_clear_depth(light2d_shadow_rend_chain, 1.0);

                        je_graphic_cmd_bind_uniform_buffer(light2d_shadow_rend_chain,
                            current_camera.projection->default_uniform_buffer->resource());

                        const shadow_pass_set& shadow_passes =
                            shadow_passes_for(lightarch.parallel != nullptr);

                        const bool topdown_light = lightarch.topdown != nullptr;
                        const shadow_depth_mapping& shadow_depth =
                            topdown_light ? shadow_depth_y : shadow_depth_z;
                        // 统一 key 模式下 TopDown=1 / 普通=0；逐顶点 key
                        // 由 +2 位标记（0/1 统一，2/3 逐顶点）
                        const float shadow_key_base_mode = topdown_light ? 1.f : 0.f;

                        // 本光源待绘制的接触阴影：推迟到其余贡献全部写入后
                        // 统一绘制，避免覆盖写入的自阴影接收面擦掉位于其
                        // 后方的接触阴影
                        std::vector<const block2d_arch*> pending_sprite_shadows;

                        for (auto& blockarch : m_blocks)
                        {
                            const float block_range = 0.5f *
                                get_entity_size(*blockarch.translation, blockarch.shape->vertex).length();

                            const auto l2b_distance = (math::vec2(blockarch.translation->world_position) -
                                math::vec2(shadow_cast_pos))
                                .length();

                            if (lightarch.parallel != nullptr || l2b_distance <= block_range + light_range)
                            {
                                // 实体级 painter key：TopDown=包围盒底边世界 Y，
                                // 普通 2D=实体世界 Z。逐段 key（瓦片轮廓）只在
                                // TopDown 下用顶点世界 Y 覆盖实体 key；普通光源
                                // 保持实体 Z，与同层 SelfShadow 的 surface key
                                // 只差固定的半层偏移，便于后者清除自遮挡。
                                const float shadow_entity_key = topdown_light
                                    ? blockarch.bottom_world_center.y
                                    : blockarch.translation->world_position.z;
                                const math::vec4 shadow_key(
                                    shadow_entity_key,
                                    shadow_depth.scale,
                                    shadow_depth.bias,
                                    shadow_key_base_mode);
                                const float block_shadow_mode = shadow_key_base_mode
                                    + (topdown_light
                                        && blockarch.blockshadow != nullptr
                                        && blockarch.blockshadow->vertex_priority ? 2.f : 0.f);
                                const math::vec4 block_shadow_key(
                                    shadow_entity_key,
                                    shadow_depth.scale,
                                    shadow_depth.bias,
                                    block_shadow_mode);

                                bool light_is_above_block = lightarch.topdown == nullptr
                                    ? blockarch.translation->world_position.z >= shadow_cast_pos.z
                                    : blockarch.bottom_world_center.y >= shadow_cast_pos.y;

                                if (blockarch.blockshadow != nullptr
                                    && blockarch.blockshadow->factor > 0.f
                                    && (!light_is_above_block || !blockarch.blockshadow->auto_disable)
                                    && blockarch.blockshadow->shape.m_block_mesh.has_value())
                                {
                                    // BlockShadow 用 (0,-1,0) 作为平行光方向参考，
                                    // 通过 local_scale.x 传递阴影权重，local_scale.y
                                    // 传递本光源的背光挤出长度（世界单位），
                                    // color.w 固定为 1。
                                    const auto& pass = blockarch.blockshadow->reverse
                                        ? shadow_passes.reverse_normal
                                        : shadow_passes.normal;

                                    draw_shadow_occluder(
                                        light2d_shadow_rend_chain,
                                        pass,
                                        blockarch.blockshadow->shape.m_block_mesh.value(),
                                        blockarch.translation,
                                        lightarch,
                                        shadow_cast_pos,
                                        MAT4_VIEW, MAT4_VP,
                                        math::vec3(
                                            blockarch.blockshadow->factor,
                                            lightarch.shadowbuffer->block_shadow_length,
                                            0.f),
                                        /*color_w=*/1.f,
                                        math::vec3(0.f, -1.f, 0.f),
                                        blockarch.textures,
                                        block_shadow_key);
                                }
                                if (blockarch.shapeshadow != nullptr
                                    && blockarch.shapeshadow->factor > 0.f
                                    && (light_is_above_block || !blockarch.shapeshadow->auto_disable))
                                {
                                    assert(blockarch.shape != nullptr);
                                    bind_shadow_texture(
                                        light2d_shadow_rend_chain, blockarch.textures);

                                    // ShapeShadow 通过 local_scale.x 传权重，.yz 传 tiling_scale，
                                    // color.w 传 distance，平行光方向参考 (0,-1,0)。
                                    draw_shadow_occluder(
                                        light2d_shadow_rend_chain,
                                        shadow_passes.shape,
                                        m_defaults.shape_of(blockarch.shape->vertex),
                                        blockarch.translation,
                                        lightarch,
                                        shadow_cast_pos,
                                        MAT4_VIEW, MAT4_VP,
                                        math::vec3(
                                            blockarch.shapeshadow->factor,
                                            blockarch.shapeshadow->tiling_scale.x,
                                            blockarch.shapeshadow->tiling_scale.y),
                                        blockarch.shapeshadow->distance,
                                        math::vec3(0.f, -1.f, 0.f),
                                        blockarch.textures,
                                        shadow_key);
                                }
                                if (blockarch.wallshadow != nullptr && blockarch.wallshadow->factor > 0.f)
                                {
                                    bind_shadow_texture(
                                        light2d_shadow_rend_chain, blockarch.textures);

                                    // WallShadow（墙状遮挡）通过 local_scale.x 传权重，.yz 预留，
                                    // color.w 传 distance，平行光方向参考 (0,-1,0)。
                                    draw_shadow_occluder(
                                        light2d_shadow_rend_chain,
                                        shadow_passes.wall,
                                        m_resources.wall_shadow_strips,
                                        blockarch.translation,
                                        lightarch,
                                        shadow_cast_pos,
                                        MAT4_VIEW, MAT4_VP,
                                        math::vec3(blockarch.wallshadow->factor, 0.f, 0.f),
                                        blockarch.wallshadow->distance,
                                        math::vec3(0.f, -1.f, 0.f),
                                        blockarch.textures,
                                        shadow_key);
                                }
                                if (blockarch.spriteshadow != nullptr && blockarch.spriteshadow->factor > 0.f)
                                    // 接触阴影不写深度、只 MAX 叠加，若在此处按实体
                                    // 顺序绘制，随后绘制的自阴影接收面（覆盖写入）
                                    // 会把位于它后方的接触阴影一并擦掉。推迟到本
                                    // 光源其余全部贡献之后统一绘制，见下方循环。
                                    pending_sprite_shadows.push_back(&blockarch);

                                // 自阴影紧随本实体的遮挡物绘制；逐像素胜负由
                                // painter key 深度决定，与绘制先后无关。
                                if (blockarch.selfshadow != nullptr)
                                    draw_self_shadow(
                                        light2d_shadow_rend_chain,
                                        blockarch,
                                        topdown_light,
                                        shadow_depth,
                                        shadow_cast_pos,
                                        MAT4_VIEW, MAT4_VP,
                                        shadow_key_base_mode);
                            }
                        }

                        // 接触阴影最后绘制：它只做 MAX 叠加、不写深度，
                        // 仅按 painter key 深度测试被更靠前的贡献遮挡。
                        // 先让所有遮挡阴影与自阴影接收面写入颜色/深度，
                        // 后方接收面的覆盖写入便擦不掉接触阴影；更靠前
                        // 的接收面/遮挡阴影已占据更小深度，仍会正确挡住它。
                        for (const block2d_arch* blockarch : pending_sprite_shadows)
                        {
                            const float shadow_entity_key = topdown_light
                                ? blockarch->bottom_world_center.y
                                : blockarch->translation->world_position.z;
                            const math::vec4 shadow_key(
                                shadow_entity_key,
                                shadow_depth.scale,
                                shadow_depth.bias,
                                shadow_key_base_mode);

                            draw_sprite_shadow(
                                light2d_shadow_rend_chain,
                                shadow_passes,
                                *blockarch,
                                lightarch,
                                shadow_cast_pos,
                                MAT4_VIEW, MAT4_VP,
                                shadow_key);
                        }
                    }
                }

                graphic::framebuffer* post_rend_target_fb =
                    current_camera.light2DPostPass->post_rend_target.value().get();
                rend_chain = begin_camera_pass(
                    current_camera, /*viewport=*/nullptr,
                    post_rend_target_fb,
                    post_rend_target_fb->width(), post_rend_target_fb->height());

                const float clear_zero[4] = { 0.f, 0.f, 0.f, 0.f };
                const float clear_infi[4] = { 0.f, 0.f, current_camera.projection->zfar, 0.f };
                const float clear_norm[4] = { 0.f, 0.f, 1.f, 1.f };

                // 用 zero 清空自发光通道
                je_graphic_cmd_clear_color(rend_chain, 1, clear_zero);
                // 用 infi 清空距离通道
                je_graphic_cmd_clear_color(rend_chain, 2, clear_infi);
                // 用 norm 清空法线通道
                je_graphic_cmd_clear_color(rend_chain, 3, clear_norm);
            }
            else
            {
                rend_chain = begin_camera_pass(
                    current_camera, current_camera.viewport,
                    rend_aim_buffer, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);
            }

            // Walk through all entities, rend them to target buffer(include L2DCamera/R2Buf/Screen).
            draw_world_renderers(
                rend_chain, m_renderers, current_camera.frustumCulling, MAT4_VIEW, MAT4_VP);

            if (current_camera.light2DPostPass != nullptr && current_camera.shaders != nullptr)
            {
                assert(current_camera.light2DPostPass->post_rend_target.has_value()
                    && current_camera.light2DPostPass->post_light_target.has_value());

                // Rend Light result to target buffer.
                graphic::framebuffer* post_light_target_fb =
                    current_camera.light2DPostPass->post_light_target.value().get();
                je_gl_command_buffer_t* light2d_light_effect_rend_chain = new_rend_chain_for_camera(
                    current_camera.branchPipeline, /*viewport=*/nullptr,
                    post_light_target_fb,
                    post_light_target_fb->width(), post_light_target_fb->height());

                const float clear_color[] = { 0.f, 0.f, 0.f, 0.f };
                je_graphic_cmd_clear_color(light2d_light_effect_rend_chain, 0, clear_color);
                je_graphic_cmd_bind_uniform_buffer(light2d_light_effect_rend_chain,
                    current_camera.projection->default_uniform_buffer->resource());

                auto* post_rend_target_frame_buffer = current_camera.light2DPostPass->post_rend_target.value().get();
                auto* diffuse_attachment = post_rend_target_frame_buffer->get_attachment(0).value()->resource();
                auto* emissive_attachment = post_rend_target_frame_buffer->get_attachment(1).value()->resource();
                auto* viewpos_attachment = post_rend_target_frame_buffer->get_attachment(2).value()->resource();
                auto* viewnorm_attachment = post_rend_target_frame_buffer->get_attachment(3).value()->resource();

                for (auto* light2d_p : _2dlight_after_culling)
                {
                    auto& light2d = *light2d_p;

                    assert(light2d.translation != nullptr
                        && light2d.shaders != nullptr
                        && light2d.shape != nullptr);

                    // bind_entity_textures allocates the group, binds default at slot 0,
                    // and binds user textures (which may override slot 0 or other slots).
                    auto bound = bind_entity_textures(
                        light2d_light_effect_rend_chain, light2d.textures, nullptr);

                    // 绑定 G-buffer 通道（漫反射 / 自发光 / 视空间坐标 / 视空间法线 / 阴影）到 4..8 槽
                    je_graphic_cmd_bind_texture_2d(light2d_light_effect_rend_chain, JE_LIGHT2D_DEFER_0 + 0, diffuse_attachment);
                    je_graphic_cmd_bind_texture_2d(light2d_light_effect_rend_chain, JE_LIGHT2D_DEFER_0 + 1, emissive_attachment);
                    je_graphic_cmd_bind_texture_2d(light2d_light_effect_rend_chain, JE_LIGHT2D_DEFER_0 + 2, viewpos_attachment);
                    je_graphic_cmd_bind_texture_2d(light2d_light_effect_rend_chain, JE_LIGHT2D_DEFER_0 + 3, viewnorm_attachment);
                    je_graphic_cmd_bind_texture_2d(
                        light2d_light_effect_rend_chain, JE_LIGHT2D_DEFER_0 + 4,
                        light2d.shadowbuffer != nullptr
                        ? light2d.shadowbuffer->buffer.value()->get_attachment(0).value()->resource()
                        : m_resources.no_shadow->resource());

                    // 开始渲染光照！光照本身从光源实体原始 Translation
                    // 位置渲染；Light2D::ShadowOffset 只影响阴影计算点
                    //（见上方阴影 pass），不经过这里
                    const float(&MAT4_MODEL)[4][4] = light2d.translation->object2world;
                    float MAT4_MV[4][4], MAT4_MVP[4][4];
                    math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);
                    math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);

                    const auto& drawing_mesh = light2d.range != nullptr
                        ? light2d.range->shape.m_light_mesh
                        : light2d.shape->vertex;

                    // 传入 Light2D 所需的颜色、衰减信息
                    math::vec4 light_color =
                        light2d.color == nullptr
                        ? math::vec4(1.f, 1.f, 1.f, 1.f)
                        : light2d.color->color.to_linear();
                    if (light2d.gain != nullptr)
                        light_color.w *= light2d.gain->gain;

                    // 阴影缓冲区分辨率（无阴影时传 1x1）
                    math::vec2 light2d_resolution(1.f, 1.f);
                    if (light2d.shadowbuffer != nullptr)
                    {
                        light2d_resolution.x = (float)scaled_buffer_size(
                            m_window_width, light2d.shadowbuffer->resolution_ratio);
                        light2d_resolution.y = (float)scaled_buffer_size(
                            m_window_height, light2d.shadowbuffer->resolution_ratio);
                    }

                    // 衰减系数仅对 Point / Range 光源存在
                    float light2d_decay_value = 0.f;
                    const float* light2d_decay_ptr = nullptr;
                    if (light2d.point != nullptr)
                    {
                        light2d_decay_value = light2d.point->decay;
                        light2d_decay_ptr = &light2d_decay_value;
                    }
                    else if (light2d.range != nullptr)
                    {
                        light2d_decay_value = light2d.range->decay;
                        light2d_decay_ptr = &light2d_decay_value;
                    }

                    pass_uniforms_t u;
                    u.m = MAT4_MODEL;
                    u.mv = MAT4_MV;
                    u.mvp = MAT4_MVP;
                    u.local_scale = light2d.translation->local_scale;
                    u.tiling = bound.tiling;
                    u.offset = bound.offset;
                    u.color = light_color;
                    u.light2d_resolution = &light2d_resolution;
                    u.light2d_decay = light2d_decay_ptr;

                    draw_shader_passes(
                        light2d_light_effect_rend_chain,
                        m_defaults.shape_of(drawing_mesh),
                        m_defaults.shaders_of(*light2d.shaders),
                        u);
                }

                // Rend final result color to screen.
                je_gl_command_buffer_t* final_target_rend_chain = begin_camera_pass(
                    current_camera, current_camera.viewport,
                    rend_aim_buffer, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                // 槽 0：光照合成结果；槽 4..7：G-buffer 通道
                je_graphic_cmd_bind_texture_2d(final_target_rend_chain, 0,
                    current_camera.light2DPostPass->post_light_target.value()->get_attachment(0).value()->resource());
                je_graphic_cmd_bind_texture_2d(final_target_rend_chain, JE_LIGHT2D_DEFER_0 + 0,
                    post_rend_target_frame_buffer->get_attachment(0).value()->resource());
                je_graphic_cmd_bind_texture_2d(final_target_rend_chain, JE_LIGHT2D_DEFER_0 + 1,
                    post_rend_target_frame_buffer->get_attachment(1).value()->resource());
                je_graphic_cmd_bind_texture_2d(final_target_rend_chain, JE_LIGHT2D_DEFER_0 + 2,
                    post_rend_target_frame_buffer->get_attachment(2).value()->resource());
                je_graphic_cmd_bind_texture_2d(final_target_rend_chain, JE_LIGHT2D_DEFER_0 + 3,
                    post_rend_target_frame_buffer->get_attachment(3).value()->resource());

                // 相机自带纹理（覆盖槽位由 m_pass_id 决定）
                math::vec2 final_tiling(1.f, 1.f), final_offset(0.f, 0.f);
                if (current_camera.textures != nullptr)
                {
                    final_tiling = current_camera.textures->tiling;
                    final_offset = current_camera.textures->offset;
                    for (auto& texture : current_camera.textures->textures)
                        je_graphic_cmd_bind_texture_2d(final_target_rend_chain, texture.m_pass_id, texture.m_texture->resource());
                }

                math::vec2 post_light_resolution(
                    (float)post_light_target_fb->width(),
                    (float)post_light_target_fb->height());
                math::vec4 final_color = current_camera.color != nullptr
                    ? current_camera.color->color.to_linear()
                    : math::vec4(1.f, 1.f, 1.f, 1.f);

                // 注意：合成 pass 不设置 m/mv/mvp/local_scale（这些 shader 不使用它们），
                // 仅设置 light2d_resolution / tiling / offset / color。
                auto* vertex_resource = m_resources.screen_quad->resource();
                for (auto& shader_pass : m_defaults.shaders_of(*current_camera.shaders))
                {
                    auto* pipeline = pipeline_for(
                        m_defaults.pass_shader_of(shader_pass), vertex_resource);
                    je_graphic_cmd_bind_pipeline(final_target_rend_chain, pipeline);
                    je_graphic_cmd_bind_vertex(final_target_rend_chain, vertex_resource);
                    render_builtin_uniforms uniforms(final_target_rend_chain, pipeline);

                    JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_light2d_resolution, float2,
                        post_light_resolution.x, post_light_resolution.y);
                    JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_uv_tiling, float2, final_tiling.x, final_tiling.y);
                    JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_uv_offset, float2, final_offset.x, final_offset.y);
                    JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_color, float4,
                        final_color.x, final_color.y, final_color.z, final_color.w);

                    je_graphic_cmd_draw_indexed(
                        final_target_rend_chain,
                        (uint32_t)vertex_resource->m_index_count, 0, 1);
                }
            } // Finish for Light2d effect.
            else if (current_camera.light2DPostPass != nullptr)
            {
                // 挂了 Light2D::CameraPostPass 但没有 Renderer::Shaders：
                // G-buffer 照常渲染，但没有任何合成 pass，画面不会呈现。
                debug::logwarn(
                    "Camera has Light2D::CameraPostPass but no Renderer::Shaders, "
                    "light2d composition is skipped.");
            }
        }
    }
}
