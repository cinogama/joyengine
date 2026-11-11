#define JE_IMPL
#include "jeecs/basic.hpp"
#include "jeecs/clock.hpp"
#include "jeecs/components.hpp"
#include "jeecs/game.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/input.hpp"
#include "jeecs/io.hpp"
#include "jeecs/math.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs_core_graphic_system.hpp"
#include "jeecs_core_render_helpers.hpp"

namespace jeecs::graphic
{
    using namespace slice_requirement;

    namespace
    {
        inline const float default_shape_quad_data[] = {
            /* vpos, vuv, vnormal, vtangent */
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f, 0.0f,
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f, 0.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f, 0.0f,
        };

        je_gl_topology_t topology_of(const je_gl_vertex_t* vertex)
        {
            switch (vertex->m_type)
            {
            case JE_GL_VERTEX_TYPE_LINESTRIP:
                return je_gl_topology_t::LINE_STRIP;
            case JE_GL_VERTEX_TYPE_TRIANGLES:
                return je_gl_topology_t::TRIANGLES;
            case JE_GL_VERTEX_TYPE_TRIANGLESTRIP:
                return je_gl_topology_t::TRIANGLE_STRIP;
            default:
                assert(false);
                return je_gl_topology_t::TRIANGLES;
            }
        }

        // Clear attachment 0 if the camera requested a clear color.
        void clear_color_if_any(
            je_gl_command_buffer_t* cmd, /* OPTIONAL */ const Camera::Clear* clear)
        {
            if (clear != nullptr)
            {
                const math::vec4 clear_color = clear->color.to_linear();
                const float clear_buffer_color[] = {
                    clear_color.x, clear_color.y, clear_color.z, clear_color.w };
                je_graphic_cmd_clear_color(cmd, 0, clear_buffer_color);
            }
        }
    }

    namespace
    {
        // 默认棋盘格纹理的创建期像素（2x2 RGBA，自底向上行序）
        const je_gl_texture_pixel_data_t s_default_texture_pixels[2 * 2 * 4] = {
            // row 0 (bottom): (0,0) magenta, (1,0) dark gray
            255, 64, 255, 255, 64, 64, 64, 255,
            // row 1 (top): (0,1) dark gray, (1,1) magenta
            64, 64, 64, 255, 255, 64, 255, 255,
        };
    }

    DefaultResources::DefaultResources()
        : default_shape_quad{
              graphic::vertex::create(
                  JE_GL_VERTEX_TYPE_TRIANGLESTRIP,
                  default_shape_quad_data,
                  sizeof(default_shape_quad_data),
                  {0, 1, 2, 3},
                  {
                      {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3},
                      {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 2},
                      {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3},
                      {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3},
                  })
                  .value() }
        , default_shader{ jeecs::graphic::shader::load(
              nullptr,
              "!/builtin/shader/internal/builtin_default.shader")
              .value() }
        , default_texture{ graphic::texture2d::create(
              2, 2, JE_GL_TEXTURE_FORMAT_RGBA, s_default_texture_pixels) }
        , default_shaders_list{ default_shader }
    {
    }

    const basic::resource<graphic::vertex>& DefaultResources::shape_of(
        const basic::optional<basic::resource<graphic::vertex>>& vertex) const
    {
        return vertex.has_value() ? vertex.value() : default_shape_quad;
    }

    const basic::vector<basic::resource<graphic::shader>>& DefaultResources::shaders_of(
        const Shaders& shaders) const
    {
        return shaders.shaders.empty() ? default_shaders_list : shaders.shaders;
    }

    graphic::shader* DefaultResources::pass_shader_of(
        const basic::resource<graphic::shader>& pass) const
    {
        return pass->m_builtin != nullptr ? pass.get() : default_shader.get();
    }

    GraphicPipelineBase::GraphicPipelineBase(game_world w)
        : BasePipelineInterface(w, nullptr)
        , m_window_width(0)
        , m_window_height(0)
    {
    }

    GraphicPipelineBase::~GraphicPipelineBase()
    {
        for (auto& [key, pipeline] : m_pipeline_cache)
            je_graphic_pipeline_close(pipeline);
        m_pipeline_cache.clear();
    }

    void GraphicPipelineBase::begin_frame()
    {
        const auto window_size = jeecs::input::windowsize();
        m_window_width = (size_t)window_size.x;
        m_window_height = (size_t)window_size.y;

        this->branch_allocate_begin();
        update_camera_matrices();
    }

    void GraphicPipelineBase::end_frame()
    {
        this->branch_allocate_end();
    }

    math::vec4 GraphicPipelineBase::current_shader_time()
    {
        const double current_time = je_clock_time();
        return math::vec4(
            (float)current_time,
            (float)abs(2.0 * (current_time * 2.0 - double(int(current_time * 2.0)) - 0.5)),
            (float)abs(2.0 * (current_time - double(int(current_time)) - 0.5)),
            (float)abs(2.0 * (current_time / 2.0 - double(int(current_time / 2.0)) - 0.5)));
    }

    void GraphicPipelineBase::collect_cameras(std::vector<camera_arch>& cameras)
    {
        cameras.clear();
        for (auto&& [
            projection,
            rendqueue,
            cameraviewport,
            rendbuf,
            frustumCulling,
            clear
        ] :
        query<
            view typesof(
                Projection&,
                Rendqueue*,
                Viewport*,
                RendToFramebuffer*,
                FrustumCulling*,
                Clear*
            )
        >())
        {
            auto* branch = this->allocate_branch(rendqueue == nullptr ? 0 : rendqueue->rend_queue);
            cameras.push_back(
                camera_arch{
                    branch, rendqueue, &projection, cameraviewport, rendbuf, frustumCulling, clear
                });
        }
        sort_by_rend_queue(cameras);
    }

    std::optional<GraphicPipelineBase::camera_target> GraphicPipelineBase::resolve_camera_target(
        const camera_arch& camera) const
    {
        graphic::framebuffer* framebuffer = nullptr;
        if (camera.rendToFramebuffer != nullptr)
        {
            if (!camera.rendToFramebuffer->framebuffer.has_value())
                return std::nullopt;
            framebuffer = camera.rendToFramebuffer->framebuffer->get();
        }

        const size_t target_width =
            framebuffer != nullptr ? framebuffer->width() : m_window_width;
        const size_t target_height =
            framebuffer != nullptr ? framebuffer->height() : m_window_height;

        // 目标尺寸为 0（窗口最小化）时没有可绘制区域：本帧跳过该相机。
        // 渲染线程在同样情况下（update_result::PAUSE）也会丢弃以屏幕为
        // 目标的绘制命令，游戏线程侧因此不必录制。
        if (target_width == 0 || target_height == 0)
            return std::nullopt;

        return camera_target{ framebuffer, target_width, target_height };
    }

    je_gl_command_buffer_t* GraphicPipelineBase::begin_camera_pass(
        const camera_arch& camera,
        const Viewport* viewport,
        graphic::framebuffer* framebuffer,
        size_t width, size_t height)
    {
        je_gl_command_buffer_t* cmd = new_rend_chain_for_camera(
            camera.branchPipeline, viewport, framebuffer, width, height);

        clear_color_if_any(cmd, camera.clear);
        je_graphic_cmd_clear_depth(cmd, 1.0);
        je_graphic_cmd_bind_uniform_buffer(cmd,
            camera.projection->default_uniform_buffer->resource());
        return cmd;
    }

    je_gl_command_buffer_t* GraphicPipelineBase::new_rend_chain_for_camera(
        je_render_branch_t* branch,
        const Viewport* viewport,
        graphic::framebuffer* rend_aim_buffer,
        size_t buffer_w, size_t buffer_h)
    {
        // 目标尺寸恒为正：窗口最小化（尺寸为 0）的相机已由
        // resolve_camera_target 返回 nullopt 提前跳过。
        assert(buffer_w > 0 && buffer_h > 0);

        je_gl_frame_buffer_t* fb =
            rend_aim_buffer ? rend_aim_buffer->resource() : nullptr;

        je_gl_viewport_t vp{};
        if (viewport != nullptr)
        {
            vp.x = (int32_t)(viewport->viewport.x * (float)buffer_w);
            vp.y = (int32_t)(viewport->viewport.y * (float)buffer_h);
            vp.width = (uint32_t)(viewport->viewport.z * (float)buffer_w);
            vp.height = (uint32_t)(viewport->viewport.w * (float)buffer_h);
        }
        else
        {
            vp.x = 0;
            vp.y = 0;
            vp.width = (uint32_t)buffer_w;
            vp.height = (uint32_t)buffer_h;
        }

        assert(vp.width > 0 && vp.height > 0);
        return je_graphic_branch_new_command_buffer(branch, fb, &vp);
    }

    void GraphicPipelineBase::update_default_uniform_buffer(
        const Projection& projection,
        const float(&view)[4][4],
        const float(&proj)[4][4],
        const float(&vp)[4][4],
        const math::vec4& shader_time)
    {
        // 录制本帧的完整一致变量内容（更新只对包含该命令的帧生效）
        graphic::BasePipelineInterface::default_uniform_buffer_data_t data;

        memcpy(data.m_v_float4x4, view, sizeof(view));
        memcpy(data.m_p_float4x4, proj, sizeof(proj));
        memcpy(data.m_vp_float4x4, vp, sizeof(vp));
        data.m_time[0] = shader_time.x;
        data.m_time[1] = shader_time.y;
        data.m_time[2] = shader_time.z;
        data.m_time[3] = shader_time.w;

        projection.default_uniform_buffer->update_buffer(
            update_command(), &data, sizeof(data));
    }

    math::vec3 GraphicPipelineBase::get_entity_size(
        const Transform::Translation& trans,
        const basic::optional<basic::resource<graphic::vertex>>& mesh)
    {
        math::vec3 size = trans.local_scale;

        const auto* const raw_vertex_data = m_defaults.shape_of(mesh)->resource();
        size.x *= 2.0f * std::max(abs(raw_vertex_data->m_x_max), abs(raw_vertex_data->m_x_min));
        size.y *= 2.0f * std::max(abs(raw_vertex_data->m_y_max), abs(raw_vertex_data->m_y_min));
        size.z *= 2.0f * std::max(abs(raw_vertex_data->m_z_max), abs(raw_vertex_data->m_z_min));

        return size;
    }

    je_gl_pipeline_t* GraphicPipelineBase::pipeline_for(
        graphic::shader* shader, je_gl_vertex_t* vertex)
    {
        assert(shader != nullptr);
        assert(vertex != nullptr);

        const auto topology = topology_of(vertex);
        const auto key = std::make_pair(
            shader->resource(), (uint8_t)topology);

        auto found = m_pipeline_cache.find(key);
        if (found != m_pipeline_cache.end())
            return found->second;

        je_gl_pipeline_state_t state =
            je_graphic_pipeline_state_from_shader(shader->resource());
        state.m_topology = topology;

        je_gl_pipeline_desc_t desc{};
        desc.m_shader = shader->resource();
        desc.m_state = &state;

        je_gl_pipeline_t* pipeline =
            je_graphic_pipeline_create(graphic_context(), &desc);
        m_pipeline_cache.emplace(key, pipeline);
        return pipeline;
    }

    GraphicPipelineBase::bound_textures_t GraphicPipelineBase::bind_entity_textures(je_gl_command_buffer_t* cmd, const Textures2D* textures,
        const Textures2DArray* textures_array)
    {
        bound_textures_t result{
            math::vec2(1.f, 1.f), math::vec2(0.f, 0.f) };

        je_graphic_cmd_bind_texture_2d(
            cmd, 0, m_defaults.default_texture->resource());

        if (textures != nullptr)
        {
            result.tiling = textures->tiling;
            result.offset = textures->offset;
            for (auto& texture : textures->textures)
                je_graphic_cmd_bind_texture_2d(
                    cmd, texture.m_pass_id, texture.m_texture->resource());
        }

        // 数组纹理在 2D 绑定之后写入同通道（后写覆盖，各后端语义一致）
        if (textures_array != nullptr)
            for (auto& texture : textures_array->textures)
                je_graphic_cmd_bind_texture_array(
                    cmd, texture.m_pass_id, texture.m_texture->resource());
        return result;
    }

    void GraphicPipelineBase::draw_shader_passes(
        je_gl_command_buffer_t* cmd,
        const basic::resource<graphic::vertex>& shape,
        const basic::vector<basic::resource<graphic::shader>>& shaders,
        const pass_uniforms_t& u)
    {
        auto* vertex_resource = shape->resource();

        for (auto& shader_pass : shaders)
        {
            auto* pipeline = pipeline_for(
                m_defaults.pass_shader_of(shader_pass), vertex_resource);
            je_graphic_cmd_bind_pipeline(cmd, pipeline);
            je_graphic_cmd_bind_vertex(cmd, vertex_resource);
            render_builtin_uniforms uniforms(cmd, pipeline);

            if (u.m != nullptr)
                JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_model, float4x4, u.m);
            if (u.mv != nullptr)
                JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_model_view, float4x4, u.mv);
            if (u.mvp != nullptr)
                JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_model_view_projection, float4x4, u.mvp);

            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_local_scale, float3,
                u.local_scale.x, u.local_scale.y, u.local_scale.z);

            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_uv_tiling, float2, u.tiling.x, u.tiling.y);
            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_uv_offset, float2, u.offset.x, u.offset.y);

            JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_color, float4,
                u.color.x, u.color.y, u.color.z, u.color.w);

            if (u.light2d_resolution != nullptr)
                JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_light2d_resolution, float2,
                    u.light2d_resolution->x, u.light2d_resolution->y);
            if (u.light2d_decay != nullptr)
                JE_CHECK_NEED_AND_SET_UNIFORM(uniforms, m_light2d_decay, float, *u.light2d_decay);

            je_graphic_cmd_draw_indexed(
                cmd, (uint32_t)vertex_resource->m_index_count, 0, 1);
        }
    }

    void GraphicPipelineBase::draw_world_renderers(
        je_gl_command_buffer_t* cmd,
        const std::vector<world_renderer_arch>& renderers,
        const FrustumCulling* frustum_culling,
        const float(&view)[4][4],
        const float(&vp)[4][4])
    {
        for (auto& rendentity : renderers)
        {
            assert(rendentity.translation != nullptr
                && rendentity.shaders != nullptr
                && rendentity.shape != nullptr);

            const float entity_range = 0.5f *
                get_entity_size(*rendentity.translation, rendentity.shape->vertex).length();

            if (frustum_culling != nullptr)
            {
                if (false == frustum_culling->test_circle(
                    rendentity.translation->world_position, entity_range))
                    continue;
            }

            const float(&MAT4_MODEL)[4][4] = rendentity.translation->object2world;
            float MAT4_MV[4][4], MAT4_MVP[4][4];
            math::mat4xmat4(MAT4_MV, view, MAT4_MODEL);
            math::mat4xmat4(MAT4_MVP, vp, MAT4_MODEL);

            auto bound = bind_entity_textures(cmd, rendentity.textures, rendentity.textures_array);

            pass_uniforms_t u;
            u.m = MAT4_MODEL;
            u.mv = MAT4_MV;
            u.mvp = MAT4_MVP;
            u.local_scale = rendentity.translation->local_scale;
            u.tiling = bound.tiling;
            u.offset = bound.offset;
            if (rendentity.color != nullptr)
                u.color = rendentity.color->color.to_linear();

            draw_shader_passes(cmd,
                m_defaults.shape_of(rendentity.shape->vertex),
                m_defaults.shaders_of(*rendentity.shaders),
                u);
        }
    }

    void GraphicPipelineBase::update_camera_matrices()
    {
        for (auto&& [
            translation,
            projection,
            ortho,
            perspec,
            viewport,
            rendbuf,
            frustumCulling
        ] :
        query<
            view typesof(
                Translation&,
                Projection&,
                OrthoProjection*,
                PerspectiveProjection*,
                Viewport*,
                RendToFramebuffer*,
                FrustumCulling*
            ),
            anyof typesof(
                OrthoProjection,
                PerspectiveProjection
            )
        >())
        {
            graphic::framebuffer* rend_aim_buffer =
                rendbuf && rendbuf->framebuffer.has_value()
                ? rendbuf->framebuffer.value().get()
                : nullptr;

            const size_t
                RENDAIMBUFFER_WIDTH =
                (size_t)llround(
                    (viewport ? viewport->viewport.z : 1.0f) *
                    (rend_aim_buffer ? rend_aim_buffer->width() : m_window_width)),
                RENDAIMBUFFER_HEIGHT =
                (size_t)llround(
                    (viewport ? viewport->viewport.w : 1.0f) *
                    (rend_aim_buffer ? rend_aim_buffer->height() : m_window_height));

            // 目标尺寸为 0（窗口最小化）：投影矩阵会退化为 NaN，视锥平面
            // 也无意义。本帧不会为该相机录制 pass（见 resolve_camera_target），
            // 矩阵组整体保留上一帧结果。
            if (RENDAIMBUFFER_WIDTH == 0 || RENDAIMBUFFER_HEIGHT == 0)
                continue;

            float mat_inv_rotation[4][4];
            translation.world_rotation.create_inv_matrix(mat_inv_rotation);

            float mat_inv_position[4][4] = {};
            mat_inv_position[0][0] = mat_inv_position[1][1] = mat_inv_position[2][2] = mat_inv_position[3][3] = 1.0f;
            mat_inv_position[3][0] = -translation.world_position.x;
            mat_inv_position[3][1] = -translation.world_position.y;
            mat_inv_position[3][2] = -translation.world_position.z;

            // TODO: Optmize
            math::mat4xmat4(projection.view, mat_inv_rotation, mat_inv_position);

            assert(ortho || perspec);
            if (ortho)
            {
                graphic::ortho_projection(projection.projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    ortho->scale, projection.znear, projection.zfar);
                graphic::ortho_inv_projection(projection.inv_projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    ortho->scale, projection.znear, projection.zfar);
            }
            else
            {
                graphic::perspective_projection(projection.projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    perspec->angle, projection.znear, projection.zfar);
                graphic::perspective_inv_projection(projection.inv_projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    perspec->angle, projection.znear, projection.zfar);
            }

            math::mat4xmat4(projection.view_projection, projection.projection, projection.view);

            if (frustumCulling != nullptr)
            {
                // Left clipping plane
                frustumCulling->set_plane_normal_and_distance(
                    0,
                    projection.view_projection[0][3] + projection.view_projection[0][0],
                    projection.view_projection[1][3] + projection.view_projection[1][0],
                    projection.view_projection[2][3] + projection.view_projection[2][0],
                    projection.view_projection[3][3] + projection.view_projection[3][0]);

                // Right clipping plane
                frustumCulling->set_plane_normal_and_distance(
                    1,
                    projection.view_projection[0][3] - projection.view_projection[0][0],
                    projection.view_projection[1][3] - projection.view_projection[1][0],
                    projection.view_projection[2][3] - projection.view_projection[2][0],
                    projection.view_projection[3][3] - projection.view_projection[3][0]);

                // Top clipping plane
                frustumCulling->set_plane_normal_and_distance(
                    2,
                    projection.view_projection[0][3] - projection.view_projection[0][1],
                    projection.view_projection[1][3] - projection.view_projection[1][1],
                    projection.view_projection[2][3] - projection.view_projection[2][1],
                    projection.view_projection[3][3] - projection.view_projection[3][1]);

                // Bottom clipping plane
                frustumCulling->set_plane_normal_and_distance(
                    3,
                    projection.view_projection[0][3] + projection.view_projection[0][1],
                    projection.view_projection[1][3] + projection.view_projection[1][1],
                    projection.view_projection[2][3] + projection.view_projection[2][1],
                    projection.view_projection[3][3] + projection.view_projection[3][1]);

                // Near clipping plane
                frustumCulling->set_plane_normal_and_distance(
                    4,
                    projection.view_projection[0][3] + projection.view_projection[0][2],
                    projection.view_projection[1][3] + projection.view_projection[1][2],
                    projection.view_projection[2][3] + projection.view_projection[2][2],
                    projection.view_projection[3][3] + projection.view_projection[3][2]);

                // Far clipping plane
                frustumCulling->set_plane_normal_and_distance(
                    5,
                    projection.view_projection[0][3] - projection.view_projection[0][2],
                    projection.view_projection[1][3] - projection.view_projection[1][2],
                    projection.view_projection[2][3] - projection.view_projection[2][2],
                    projection.view_projection[3][3] - projection.view_projection[3][2]);
            }
        }
    }

    je_gl_context_t* GraphicPipelineBase::graphic_context() const
    {
        je_gl_context_t* context = je_graphic_uhost_get_context(_m_graphic_host);
        assert(context != nullptr);
        return context;
    }
}
