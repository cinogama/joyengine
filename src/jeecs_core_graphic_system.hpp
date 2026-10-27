#pragma once

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#   error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"
#include "jeecs_core_render_helpers.hpp"
#include "jeecs_ui_text_renderer.hpp"

#include <queue>
#include <list>
#include <map>
#include <utility>

#define JE_LIGHT2D_DEFER_0 4

namespace jeecs
{
    using namespace Transform;
    using namespace Camera;
    using namespace Renderer;
    using namespace UserInterface;
    using namespace Light2D;

    using namespace slice_requirement;

    struct DefaultResources
    {
        basic::resource<graphic::vertex> default_shape_quad;
        basic::resource<graphic::shader> default_shader;
        basic::resource<graphic::texture2d> default_texture;
        basic::vector<basic::resource<graphic::shader>> default_shaders_list;

        JECS_DISABLE_MOVE_AND_COPY(DefaultResources);

        inline static const float default_shape_quad_data[] = {
            /* vpos, vuv, vnormal, vtangent */
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f, 0.0f,
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f, 0.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f, 0.0f, 0.0f,
        };

        DefaultResources()
            : default_shape_quad{
                  graphic::vertex::create(
                      jegl_vertex::TRIANGLESTRIP,
                      default_shape_quad_data,
                      sizeof(default_shape_quad_data),
                      {0, 1, 2, 3},
                      {
                          {jegl_vertex::data_type::FLOAT32, 3},
                          {jegl_vertex::data_type::FLOAT32, 2},
                          {jegl_vertex::data_type::FLOAT32, 3},
                          {jegl_vertex::data_type::FLOAT32, 3},
                      })
                      .value() },
            default_shader{ jeecs::graphic::shader::load(
                                nullptr,
                                "!/builtin/shader/internal/builtin_default.shader")
                                .value() }
, default_texture{ graphic::texture2d::create(2, 2, jegl_texture_common::format::RGBA) }
, default_shaders_list{ default_shader }
        {
            default_texture->pix(0, 0).set({ 1.f, 0.25f, 1.f, 1.f });
            default_texture->pix(1, 1).set({ 1.f, 0.25f, 1.f, 1.f });
            default_texture->pix(0, 1).set({ 0.25f, 0.25f, 0.25f, 1.f });
            default_texture->pix(1, 0).set({ 0.25f, 0.25f, 0.25f, 1.f });
        }
    };

    struct BaseImpledGraphicPipeline : public graphic::BasePipelineInterface
    {
        BaseImpledGraphicPipeline(game_world w)
            : BasePipelineInterface(w, nullptr)
        {
        }

        struct camera_arch
        {
            je_RenderBranch* branchPipeline;

            const Rendqueue* rendqueue;
            const Projection* projection;
            const Viewport* viewport;
            const RendToFramebuffer* rendToFramebuffer;
            const FrustumCulling* frustumCulling;
            const Camera::Clear* clear;

            bool operator<(const camera_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };
        struct renderer_arch
        {
            const Renderer::Color* color;
            const Rendqueue* rendqueue;
            const Translation* translation;
            const Shape* shape;
            const Shaders* shaders;
            const Textures2D* textures;

            // UI 布局量：父子层级（含祖先旋转链）已由布局阶段复合进
            // WorldLayout 的仿射系数通道，绘制端只按相机目标尺寸像素化。
            const UserInterface::Element* ui_elem = nullptr;
            const UserInterface::WorldLayout* ui_layout = nullptr;
            const UserInterface::Rotation* ui_rotation = nullptr; // 可空

            // UI 文本（逐字渲染路径）；仅 UI 渲染路径使用，为空时该实体走
            // 常规四边形路径（两者可并存：先四边形后文本，文字在背景板之上）。
            const UserInterface::Text* text = nullptr;

            // 纹理数组通道绑定（瓦片地图分片等使用）；可空
            const Textures2DArray* textures_array = nullptr;

            bool operator<(const renderer_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };

        DefaultResources m_default_resources;

        std::multiset<camera_arch> m_camera_list;
        std::multiset<renderer_arch> m_renderer_list;

        size_t WINDOWS_WIDTH = 0;
        size_t WINDOWS_HEIGHT = 0;

        void PrepareCameras()
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
                graphic::framebuffer* rend_aim_buffer =
                    rendbuf && rendbuf->framebuffer.has_value()
                    ? rendbuf->framebuffer.value().get()
                    : nullptr;

                const size_t
                    RENDAIMBUFFER_WIDTH =
                    (size_t)llround(
                        (viewport ? viewport->viewport.z : 1.0f) *
                        (rend_aim_buffer ? rend_aim_buffer->width() : WINDOWS_WIDTH)),
                    RENDAIMBUFFER_HEIGHT =
                    (size_t)llround(
                        (viewport ? viewport->viewport.w : 1.0f) *
                        (rend_aim_buffer ? rend_aim_buffer->height() : WINDOWS_HEIGHT));

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

                projection.default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_v_float4x4),
                    sizeof(projection.view),
                    projection.view);
                projection.default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_p_float4x4),
                    sizeof(projection.projection),
                    projection.projection);

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

        math::vec3 get_entity_size(
            const Transform::Translation& trans,
            const basic::optional<basic::resource<graphic::vertex>>& mesh)
        {
            math::vec3 size = trans.local_scale;

            const auto* light_shape =
                mesh.has_value() ? mesh->get() : m_default_resources.default_shape_quad.get();

            const auto* const raw_vertex_data = light_shape->resource();
            size.x *= 2.0f * std::max(abs(raw_vertex_data->m_x_max), abs(raw_vertex_data->m_x_min));
            size.y *= 2.0f * std::max(abs(raw_vertex_data->m_y_max), abs(raw_vertex_data->m_y_min));
            size.z *= 2.0f * std::max(abs(raw_vertex_data->m_z_max), abs(raw_vertex_data->m_z_min));

            return size;
        }

        /* ---------------- Shared render-pipeline helpers ---------------- */

        // Write the per-camera view/projection/VP/time quartet into the default uniform buffer.
        void update_default_uniform_buffer(
            const Projection& projection,
            const float(&view)[4][4],
            const float(&proj)[4][4],
            const float(&vp)[4][4],
            const math::vec4& shader_time)
        {
            auto* ub = projection.default_uniform_buffer.get();
            ub->update_buffer(
                offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_v_float4x4),
                sizeof(view), view);
            ub->update_buffer(
                offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_p_float4x4),
                sizeof(proj), proj);
            ub->update_buffer(
                offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_vp_float4x4),
                sizeof(vp), vp);
            ub->update_buffer(
                offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_time),
                sizeof(shader_time), &shader_time);
        }

        // Pipeline cache keyed by (shader, topology). The engine shaders carry
        // their fixed state; topology comes from the vertex resource.
        std::map<std::pair<jegl_shader*, uint8_t>, jegl_pipeline*> m_pipeline_cache;

        ~BaseImpledGraphicPipeline()
        {
            for (auto& [key, pipeline] : m_pipeline_cache)
                jegl_close_pipeline(pipeline);
            m_pipeline_cache.clear();
        }

        jegl_context* graphic_context() const
        {
            jegl_context* context = jegl_uhost_get_context(_m_graphic_host);
            assert(context != nullptr);
            return context;
        }

        static jegl_topology topology_of(const jegl_vertex* vertex)
        {
            switch (vertex->m_type)
            {
            case jegl_vertex::type::LINESTRIP:
                return jegl_topology::LINE_STRIP;
            case jegl_vertex::type::TRIANGLES:
                return jegl_topology::TRIANGLES;
            case jegl_vertex::type::TRIANGLESTRIP:
                return jegl_topology::TRIANGLE_STRIP;
            default:
                assert(false);
                return jegl_topology::TRIANGLES;
            }
        }

        jegl_pipeline* pipeline_for(
            graphic::shader* shader, jegl_vertex* vertex)
        {
            assert(shader != nullptr);
            assert(vertex != nullptr);

            const auto topology = topology_of(vertex);
            const auto key = std::make_pair(
                shader->resource(), (uint8_t)topology);

            auto found = m_pipeline_cache.find(key);
            if (found != m_pipeline_cache.end())
                return found->second;

            jegl_pipeline_state state =
                jegl_pipeline_state_from_shader(shader->resource());
            state.m_topology = topology;

            jegl_pipeline_desc desc{};
            desc.m_shader = shader->resource();
            desc.m_state = &state;

            jegl_pipeline* pipeline =
                jegl_pipeline_create(graphic_context(), &desc);
            m_pipeline_cache.emplace(key, pipeline);
            return pipeline;
        }

        // Allocate a command buffer/pass for a camera; honors optional
        // sub-viewport (normalized rect).
        jegl_command_buffer* new_rend_chain_for_camera(
            je_RenderBranch* branch,
            const Viewport* viewport,
            graphic::framebuffer* rend_aim_buffer,
            size_t buffer_w, size_t buffer_h)
        {
            assert(buffer_w > 0 && buffer_h > 0);

            jegl_frame_buffer* fb =
                rend_aim_buffer ? rend_aim_buffer->resource() : nullptr;

            jegl_viewport vp{};
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
            return jegl_branch_new_command_buffer(branch, fb, &vp);
        }

        // Clear attachment 0 if the camera requested a clear color.
        void clear_color_if_any(
            jegl_command_buffer* cmd, const Camera::Clear* clear)
        {
            if (clear != nullptr)
            {
                const math::vec4 clear_color = clear->color.to_linear();
                const float clear_buffer_color[] = {
                    clear_color.x, clear_color.y, clear_color.z, clear_color.w };
                jegl_cmd_clear_color(cmd, 0, clear_buffer_color);
            }
        }

        // Bind default texture at slot 0, then any user-supplied textures.
        // Returns the resolved tiling/offset.
        struct bound_textures_t
        {
            math::vec2 tiling;
            math::vec2 offset;
        };
        bound_textures_t bind_entity_textures(jegl_command_buffer* cmd, const Textures2D* textures,
            const Textures2DArray* textures_array)
        {
            bound_textures_t result{
                math::vec2(1.f, 1.f), math::vec2(0.f, 0.f) };

            jegl_cmd_bind_texture_2d(
                cmd, 0, m_default_resources.default_texture->resource());

            if (textures != nullptr)
            {
                result.tiling = textures->tiling;
                result.offset = textures->offset;
                for (auto& texture : textures->textures)
                    jegl_cmd_bind_texture_2d(
                        cmd, texture.m_pass_id, texture.m_texture->resource());
            }

            // 数组纹理在 2D 绑定之后写入同通道（后写覆盖，各后端语义一致）
            if (textures_array != nullptr)
                for (auto& texture : textures_array->textures)
                    jegl_cmd_bind_texture_array(
                        cmd, texture.m_pass_id, texture.m_texture->resource());
            return result;
        }

        // Optional uniforms grouped together; nullptr / unset fields are skipped.
        // Used by both world-space (Unlit / DeferLight2D main) and screen-space (UI) passes.
        struct pass_uniforms_t
        {
            const float (*m)[4] = nullptr;
            const float (*mv)[4] = nullptr;
            const float (*mvp)[4] = nullptr;
            math::vec3 local_scale = math::vec3(1.f, 1.f, 1.f);
            math::vec2 tiling = math::vec2(1.f, 1.f);
            math::vec2 offset = math::vec2(0.f, 0.f);
            math::vec4 color = math::vec4(1.f, 1.f, 1.f, 1.f);
            // Light2D-only extras; set when non-null.
            const math::vec2* light2d_resolution = nullptr;
            const float* light2d_decay = nullptr;
        };

        // Iterate shader passes of `shaders`, falling back to default shader if a pass is not builtin,
        // and upload the standard set of builtin uniforms.
        void draw_shader_passes(
            jegl_command_buffer* cmd,
            const basic::resource<graphic::vertex>& shape,
            const basic::vector<basic::resource<graphic::shader>>& shaders,
            const pass_uniforms_t& u)
        {
            auto* vertex_resource = shape->resource();

            for (auto& shader_pass : shaders)
            {
                auto* using_shader = &shader_pass;
                if (!shader_pass->m_builtin)
                    using_shader = &m_default_resources.default_shader;

                auto* pipeline = pipeline_for(
                    (*using_shader).get(), vertex_resource);

                render_draw_action draw_action(
                    cmd,
                    pipeline,
                    vertex_resource,
                    (uint32_t)vertex_resource->m_index_count,
                    1);

                if (u.m != nullptr)
                    JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_model, float4x4, u.m);
                if (u.mv != nullptr)
                    JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_model_view, float4x4, u.mv);
                if (u.mvp != nullptr)
                    JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_model_view_projection, float4x4, u.mvp);

                JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_local_scale, float3,
                    u.local_scale.x, u.local_scale.y, u.local_scale.z);

                JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_uv_tiling, float2, u.tiling.x, u.tiling.y);
                JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_uv_offset, float2, u.offset.x, u.offset.y);

                JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_color, float4,
                    u.color.x, u.color.y, u.color.z, u.color.w);

                if (u.light2d_resolution != nullptr)
                    JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_light2d_resolution, float2,
                        u.light2d_resolution->x, u.light2d_resolution->y);
                if (u.light2d_decay != nullptr)
                    JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_light2d_decay, float, *u.light2d_decay);
            }
        }

        // Draw every entity in m_renderer_list using the standard world-space pipeline
        // (frustum culling -> MV/MVP -> bind_entity_textures -> draw_shader_passes).
        // Shared between UnlitGraphicPipelineSystem and DeferLight2DGraphicPipelineSystem.
        // Takes the frustum-culling handle directly so it works with any camera archetype
        // (camera_arch / l2dcamera_arch / ...) that exposes a `frustumCulling` field.
        void draw_world_renderers(
            jegl_command_buffer* cmd,
            const FrustumCulling* frustum_culling,
            const float(&view)[4][4],
            const float(&vp)[4][4])
        {
            for (auto& rendentity : m_renderer_list)
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

                auto& drawing_shape =
                    rendentity.shape->vertex.has_value()
                    ? rendentity.shape->vertex.value()
                    : m_default_resources.default_shape_quad;
                auto& drawing_shaders =
                    rendentity.shaders->shaders.empty() == false
                    ? rendentity.shaders->shaders
                    : m_default_resources.default_shaders_list;

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

                draw_shader_passes(cmd, drawing_shape, drawing_shaders, u);
            }
        }
    };

    struct UserInterfaceGraphicPipelineSystem : public BaseImpledGraphicPipeline
    {
        // ---- 文本逐字渲染常量 ----
        // 顶点格式（与内置文本着色器的 vin 声明顺序一致）：
        // float3 位置（相机像素空间，元素矩阵已烘焙）+ float2 UV（字形图集）
        // + float4 颜色（逐字颜色 × 节点色调）。
        static constexpr size_t TEXT_VERTEX_FLOATS = 9;
        // 单个动态顶点缓冲的字形容量（8192 四边形 ≈ 1.15MB 顶点 + 192KB 索引）。
        static constexpr size_t MAX_TEXT_QUADS_PER_VB = 8192;
        // 顶点缓冲池上限：每个“连续同键（图集页, 着色器）文本运行段”占用一块，
        // 池跨帧复用；超限丢弃溢出文本并告警（正常 UI 场景为个位数）。
        static constexpr size_t MAX_TEXT_VB_COUNT = 32;

        UserInterfaceGraphicPipelineSystem(game_world w)
            : BaseImpledGraphicPipeline(w)
            // 内嵌源码与 builtin/shader/UIText.shader 保持同源；两处以相同
            // 路径加载，命中同一共享缓存实例（引擎核心自足性约定：核心
            // 管线着色器不依赖内容文件，见 DefaultResources 与 DeferLight2D）。
        {
        }

        ~UserInterfaceGraphicPipelineSystem()
        {
        }

        // ---- 文本逐字渲染运行时 ----
        // 字形图集 + 行网格布局引擎（LRU、脏检查细节见 jeecs_ui_text_renderer.hpp）。
        ui_text_renderer m_text_renderer;

        // 动态顶点缓冲池：跨帧复用；同一帧内每个运行段独占一块（command buffer
        // 记录绘制动作后图形线程滞后消费，同帧内重复更新同一缓冲会互相覆盖）。
        std::vector<basic::resource<graphic::vertex>> m_text_vb_pool;
        size_t m_text_vb_used = 0;
        bool m_warned_text_vb_exhausted = false;
        // 当前批（连续同键运行段）状态；发射端逐字追加，键变化/缓冲满/相机
        // 切换时 flush 成一次 command-buffer draw。
        std::vector<float> m_text_quad_data;
        jegl_texture_2d* m_text_batch_page = nullptr;
        graphic::shader* m_text_batch_shader = nullptr;
        basic::resource<graphic::vertex>* m_text_batch_vb = nullptr;
        size_t m_text_batch_quads = 0;

        void GraphicUpdate()
        {
            auto WINDOWS_SIZE = jeecs::input::windowsize();
            WINDOWS_WIDTH = (size_t)WINDOWS_SIZE.x;
            WINDOWS_HEIGHT = (size_t)WINDOWS_SIZE.y;

            m_camera_list.clear();
            m_renderer_list.clear();

            // MouseOn 是逐帧派生态：帧首统一清零，随后各摄像机的 UI 阶段
            // 命中时置位（只累加不清理，见 UpdateMouseOn）。
            for (auto&& [mouse_on] : query<view typesof(MouseOn&)>())
                mouse_on.is_mouse_on = false;

            this->branch_allocate_begin();

            UserInterfaceGraphicPipelineSystem::PrepareCameras();

            for (auto&& [
                projection,
                rendqueue,
                cameraviewport,
                rendbuf,
                clear
            ] :
            query<
                view typesof(
                    Projection&,
                    Rendqueue*,
                    Viewport*,
                    RendToFramebuffer*,
                    Clear*
                )
            >())
            {
                auto* branch = this->allocate_branch(rendqueue == nullptr ? 0 : rendqueue->rend_queue);
                m_camera_list.emplace(
                    camera_arch{
                        branch, rendqueue, &projection, cameraviewport, rendbuf, nullptr, clear
                    });
            }

            // 帧首推进字形图集 LRU，并按窗口分辨率重建全部文本的逐字布局
            //（布局与相机无关，相机目标分辨率差异由发射端统一缩放）。
            m_text_renderer.begin_frame();
            PrepareTextLayouts();
            m_text_vb_used = 0;

            // 收集可渲染 UI 实体：带 Shaders 且带 Shape（常规四边形）或
            // Text（逐字文本）。父子层级已由布局阶段复合进 WorldLayout 的
            // 仿射系数通道，此处不再构建节点树、不读 Anchor/LocalToParent。
            // 2D 光源由专用查询收集，此处不再单独排除。
            for (auto&& [
                elem, layout, rotation, text, texs,
                shads, shape, rendqueue, color] : query<
                view typesof(
                    Element&,
                    WorldLayout&,
                    Rotation*,
                    Text*,
                    Textures2D*,
                    Shaders&,
                    Shape*,
                    Rendqueue*,
                    Color*),
                anyof typesof(Shape, Text)
                >())
            {
                m_renderer_list.emplace(
                    renderer_arch{
                        color, rendqueue, nullptr, shape, &shads, texs,
                        &elem, &layout, rotation, text });
            }

            this->branch_allocate_end();
            DrawFrame();
        }

        // 按窗口分辨率重建全部 UI 文本的逐字布局（标记解析 → 字形度量 →
        // 折行 → 对齐 → 放置），结果缓存于 Text::runtime_layout，仅脏文本
        // 重建。实际像素字号 = 元素按窗口尺寸解析后的最终布局尺寸（基准轴：
        // width_unit 取 x 分量，否则取 y 分量——per_axis 亦取 y，文字以字高
        // 为基准），并量化为整像素以获得稳定的字形缓存命中；相机目标分辨率
        // 的差异由发射端按基准轴比例统一缩放。字号取局部（未旋转）矩形，
        // 不回写任何布局输入组件。
        void PrepareTextLayouts()
        {
            for (auto&& [text, elem, layout] : query<view typesof(
                Text&, Element&, WorldLayout&)>())
            {
                m_text_renderer.layout_text(
                    text, elem, layout,
                    (float)WINDOWS_WIDTH, (float)WINDOWS_HEIGHT);
            }
        }

        // 逐摄像机更新 MouseOn（UI 命中测试）。UI 元素矩形按本摄像机目标
        // 尺寸像素化后做含旋转的命中测试，因此必须逐摄像机执行：
        //   * render_to_window 为 false 时不处理——渲染到 Framebuffer 的
        //     摄像机是离屏目标，窗口鼠标不在其空间中；
        //   * 鼠标须落在摄像机视口（Camera::Viewport，归一化、左下原点）
        //     内；视口只裁剪绘制，不改变元素矩形（仍按目标完整尺寸解析）；
        //   * 命中只置位，不清理其他摄像机已写入的结果：帧首已统一清零，
        //     多个摄像机命中同一元素时保持 true。
        // 不参与渲染的元素（无 Shape/Text/Shaders）同样参与测试，可作为
        // 不可见按钮热区。
        void UpdateMouseOn(
            const camera_arch& camera,
            bool render_to_window,
            size_t buffer_w, size_t buffer_h)
        {
            if (!render_to_window
                || WINDOWS_WIDTH == 0 || WINDOWS_HEIGHT == 0
                || buffer_w == 0 || buffer_h == 0)
                return;

            const math::ivec2 mouse = jeecs::input::mousepos(0);

            // 窗口鼠标 -> 渲染目标空间 NDC（-1..1，y 向上），与
            // resolved_rect::hit_test 的 mouse_view_pos 约定一致。
            const math::vec2 mouse_view(
                ((float)mouse.x / (float)WINDOWS_WIDTH - 0.5f) * 2.0f,
                ((float)mouse.y / (float)WINDOWS_HEIGHT - 0.5f) * -2.0f);

            if (camera.viewport != nullptr)
            {
                const math::vec4& vp = camera.viewport->viewport;
                if (vp.z <= 0.f || vp.w <= 0.f)
                    return;

                // 视口以渲染目标左下角为原点；鼠标也折算到左下原点后
                // 做包含判定。
                const float x = (float)mouse.x / (float)WINDOWS_WIDTH;
                const float y = 1.0f - (float)mouse.y / (float)WINDOWS_HEIGHT;
                if (x < vp.x || x > vp.x + vp.z
                    || y < vp.y || y > vp.y + vp.w)
                    return;
            }

            for (auto&& [elem, layout, mouse_on] : query<view typesof(
                Element&, WorldLayout&, MouseOn&)>())
            {
                const auto rect = elem.resolve_display_rect(
                    layout, (float)buffer_w, (float)buffer_h);

                if (!rect.hit_test(
                        layout.rotation, mouse_view,
                        (float)buffer_w, (float)buffer_h))
                    continue;

                mouse_on.is_mouse_on = true;
            }
        }

        void DrawFrame()
        {
            const double current_time = je_clock_time();

            math::vec4 shader_time =
            {
                (float)current_time,
                (float)abs(2.0 * (current_time * 2.0 - double(int(current_time * 2.0)) - 0.5)),
                (float)abs(2.0 * (current_time - double(int(current_time)) - 0.5)),
                (float)abs(2.0 * (current_time / 2.0 - double(int(current_time / 2.0)) - 0.5)) };

            // UI uses an identity projection; the view matrix maps pixels to NDC (-1..1).
            const float MAT4_UI_UNIT[4][4] = {
                {1.0f, 0.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f, 0.0f},
                {0.0f, 0.0f, 1.0f, 0.0f},
                {0.0f, 0.0f, 0.0f, 1.0f},
            };
            float MAT4_UI_MODEL[4][4];
            float MAT4_UI_MV[4][4];

            for (auto& current_camera : m_camera_list)
            {
                graphic::framebuffer* rend_aim_buffer = nullptr;
                if (current_camera.rendToFramebuffer != nullptr)
                {
                    if (current_camera.rendToFramebuffer->framebuffer.has_value())
                        rend_aim_buffer = current_camera.rendToFramebuffer->framebuffer->get();
                    else
                        continue;
                }
                const size_t RENDAIMBUFFER_WIDTH = rend_aim_buffer ? rend_aim_buffer->width() : WINDOWS_WIDTH,
                    RENDAIMBUFFER_HEIGHT = rend_aim_buffer ? rend_aim_buffer->height() : WINDOWS_HEIGHT;

                // 逐摄像机更新鼠标命中状态；离屏 Framebuffer 不参与。
                UpdateMouseOn(current_camera, rend_aim_buffer == nullptr,
                    RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                const float MAT4_UI_VIEW[4][4] = {
                    {2.0f / (float)RENDAIMBUFFER_WIDTH, 0.0f, 0.0f, 0.0f},
                    {0.0f, 2.0f / (float)RENDAIMBUFFER_HEIGHT, 0.0f, 0.0f},
                    {0.0f, 0.0f, 1.0f, 0.0f},
                    {0.0f, 0.0f, 0.0f, 1.0f},
                };
                // For UI, vp == view (projection is identity), so view-projection == view.
                update_default_uniform_buffer(
                    *current_camera.projection, MAT4_UI_VIEW, MAT4_UI_UNIT, MAT4_UI_VIEW, shader_time);

                jegl_command_buffer* rend_chain = new_rend_chain_for_camera(
                    current_camera.branchPipeline, current_camera.viewport,
                    rend_aim_buffer, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                clear_color_if_any(rend_chain, current_camera.clear);
                jegl_cmd_clear_depth(rend_chain, 1.0f);
                jegl_cmd_bind_uniform_buffer(rend_chain,
                    current_camera.projection->default_uniform_buffer->resource());

                for (auto& rendentity : m_renderer_list)
                {
                    assert(rendentity.ui_elem != nullptr
                        && rendentity.ui_layout != nullptr);

                    // 布局阶段已把父子层级与祖先旋转链复合进 WorldLayout 的
                    // 仿射系数，此处只按本相机目标尺寸像素化。
                    const auto rect = rendentity.ui_elem->resolve_display_rect(
                        *rendentity.ui_layout,
                        (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT);

                    // 总旋转角 = 布局阶段缓存的 WorldLayout::rotation（祖先累计
                    // + 自身，度，quat::euler 按度消费）；祖先累计角 = 总角 −
                    // 自身角。矩阵结构与未旋转时一致
                    //（T(枢轴)·R·T(pivot_offset)·S 的展开形式）：uioffset 保证
                    // 枢轴落在其经祖先旋转链后的显示位置 A(Q)，即
                    // uioffset = D_C + (I − R(Θ_acc))·pivot_offset（D_C 为已
                    // 复合显示中心 = rect.center，R 由 vec2::rotate 按度计算）。
                    // 该矩阵把“单位矩形坐标”（±0.5 张成元素矩形）映射到相机
                    // 像素空间，常规四边形路径（着色器侧 MVP）与逐字文本路径
                    //（CPU 侧烘焙进顶点）共用同一映射。
                    const float total_angle = rendentity.ui_layout->rotation;
                    const float angle_acc = total_angle
                        - (rendentity.ui_rotation != nullptr
                            ? rendentity.ui_rotation->angle : 0.f);

                    const math::vec2 uioffset = rect.center
                        + (rect.pivot_offset
                            - rect.pivot_offset.rotate(angle_acc))
                        - math::vec2(
                            (float)RENDAIMBUFFER_WIDTH / 2.0f,
                            (float)RENDAIMBUFFER_HEIGHT / 2.0f);
                    const math::vec2& uisize = rect.size;
                    const math::vec2& uicenteroffset = rect.pivot_offset;

                    // TODO: 这里俩矩阵实际上可以优化，但是UI实际上也没有多少，暂时直接矩阵乘法也无所谓
                    // NOTE: 这里的大小和偏移大小乘二是因为一致空间是 -1 到 1，天然有一个1/2的压缩，为了保证单位正确，这里乘二
                    const float MAT4_UI_OFFSET[4][4] = {
                        {1.0f, 0.0f, 0.0f, 0.0f},
                        {0.0f, 1.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 1.0f, 0.0f},
                        {uioffset.x, uioffset.y, 0.0f, 1.0f},
                    };
                    const float MAT4_UI_SIZE[4][4] = {
                        {uisize.x, 0.0f, 0.0f, 0.0f},
                        {0.0f, uisize.y, 0.0f, 0.0f},
                        {0.0f, 0.0f, 1.0f, 0.0f},
                        {0.0f, 0.0f, 0.0f, 1.0f}
                    };
                    float MAT4_UI_ROTATION[4][4] = {
                        {1.0f, 0.0f, 0.0f, 0.0f},
                        {0.0f, 1.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 1.0f, 0.0f},
                        {0.0f, 0.0f, 0.0f, 1.0f},
                    };

                    {
                        // 旋转绕枢轴进行（角度为 0 时 quat 即单位矩阵）。
                        const float MAT4_UI_CENTER_OFFSET[4][4] = {
                            {1.0f, 0.0f, 0.0f, 0.0f},
                            {0.0f, 1.0f, 0.0f, 0.0f},
                            {0.0f, 0.0f, 1.0f, 0.0f},
                            {uicenteroffset.x, uicenteroffset.y, 0.0f, 1.0f} };
                        const float MAT4_UI_INV_CENTER_OFFSET[4][4] = {
                            {1.0f, 0.0f, 0.0f, 0.0f},
                            {0.0f, 1.0f, 0.0f, 0.0f},
                            {0.0f, 0.0f, 1.0f, 0.0f},
                            {-uicenteroffset.x, -uicenteroffset.y, 0.0f, 1.0f} };
                        math::quat q(0.0f, 0.0f, total_angle);
                        q.create_matrix(MAT4_UI_ROTATION);

                        math::mat4xmat4(MAT4_UI_MV /* tmp */, MAT4_UI_ROTATION, MAT4_UI_CENTER_OFFSET);
                        math::mat4xmat4(MAT4_UI_ROTATION, MAT4_UI_INV_CENTER_OFFSET, MAT4_UI_MV /* tmp */);
                    }
                    math::mat4xmat4(MAT4_UI_MV /* tmp */, MAT4_UI_OFFSET, MAT4_UI_ROTATION);
                    math::mat4xmat4(MAT4_UI_MODEL, MAT4_UI_MV /* tmp */, MAT4_UI_SIZE);
                    math::mat4xmat4(MAT4_UI_MV, MAT4_UI_VIEW, MAT4_UI_MODEL);

                    // 常规四边形路径：Shaders+Shape 齐备的实体（文本实体若
                    // 同时挂有两者则先画背景板，文字叠于其上）。
                    if (rendentity.shape != nullptr)
                    {
                        auto& drawing_shape =
                            rendentity.shape->vertex.has_value()
                            ? rendentity.shape->vertex.value()
                            : m_default_resources.default_shape_quad;
                        auto& drawing_shaders =
                            rendentity.shaders->shaders.empty() == false
                            ? rendentity.shaders->shaders
                            : m_default_resources.default_shaders_list;

                        auto bound = bind_entity_textures(rend_chain, rendentity.textures, rendentity.textures_array);

                        pass_uniforms_t u;
                        u.m = MAT4_UI_MODEL;
                        // UI uses MV for both mvp and mv slots.
                        u.mv = MAT4_UI_MV;
                        u.mvp = MAT4_UI_MV;
                        u.tiling = bound.tiling;
                        u.offset = bound.offset;
                        if (rendentity.color != nullptr)
                            u.color = rendentity.color->color.to_linear();

                        draw_shader_passes(rend_chain, drawing_shape, drawing_shaders, u);
                    }

                    // 逐字文本路径：字形四边形按（图集页, 着色器）合并成连续
                    // 运行段，段满/键变化/相机切换时冲刷为一次绘制调用。
                    if (rendentity.text != nullptr)
                    {
                        EmitTextGlyphs(
                            rend_chain, MAT4_UI_VIEW, MAT4_UI_MODEL,
                            *rendentity.text, *rendentity.ui_elem, rect, rendentity);
                    }
                }

                // 冲刷本相机最后一个文本运行段（批状态跨相机持有视图矩阵，
                // 必须在视图离开作用域前落盘），并放弃对当前缓冲的占用——
                // 已记录的绘制动作仍引用它，下一相机必须取新缓冲，否则
                // 同帧内重复更新同一缓冲会与图形线程的滞后消费竞争。
                FlushTextBatch(rend_chain, MAT4_UI_VIEW);
                m_text_batch_vb = nullptr;
            }
        }

        // ---- 文本逐字批处理 ----

        // 发射一个文本节点的全部字形四边形。字形放置来自布局缓存（文本
        // 空间，窗口分辨率），经 s（相机基准轴/量化字号）统一缩放、按
        // alignment 九宫格放置进元素矩形（矩形局部像素 → 单位矩形坐标），
        // 再经与四边形路径完全相同的元素矩阵烘焙成相机像素空间顶点；
        // 颜色 = 逐字标记颜色 × 节点 Renderer::Color 色调。
        void EmitTextGlyphs(
            jegl_command_buffer* chain,
            const float(&view)[4][4],
            const float(&model)[4][4],
            const UserInterface::Text& text,
            const UserInterface::Element& elem,
            const UserInterface::Element::resolved_rect& rect,
            const renderer_arch& rendentity)
        {
            const auto& layout = text.runtime_layout;
            if (!layout.valid || layout.glyphs.size() == 0)
                return;

            if (rect.size.x <= 0.f || rect.size.y <= 0.f)
                return;

            // 字号基准轴与布局一致：width_unit 取 x，否则取 y。
            const float base_axis =
                elem.unit_kind == Element::ratio_unit::width_unit
                ? rect.size.x : rect.size.y;
            if (base_axis <= 0.f || layout.key_font_px <= 0)
                return;

            auto font_holder = text.font.get_resource();
            if (!font_holder.has_value())
                return;
            auto& font_res = font_holder.value();

            // 着色器：实体 Shaders 的第一个 woshader pass（自定义文本特效），
            // 否则内置文本着色器。
            graphic::shader* shader_res = m_default_resources.default_shader.get();
            if (!rendentity.shaders->shaders.empty())
            {
                auto& user_shader = rendentity.shaders->shaders.front();
                if (user_shader->m_builtin != nullptr)
                    shader_res = user_shader.get();
            }

            // 节点色调烘焙进顶点色（批内共享一次绘制，无法逐节点传 uniform）。
            math::vec4 tint(1.f, 1.f, 1.f, 1.f);
            if (rendentity.color != nullptr)
                tint = rendentity.color->color.to_linear();

            // 文本像素 → 相机像素：布局在窗口分辨率下量化，相机目标分辨率
            // 不同时按基准轴比例统一缩放（行为与旧纹理路径等价）。
            const float s = base_axis / (float)layout.key_font_px;

            // 文本块左下角（矩形局部像素系，原点=矩形中心）与单位矩形换算。
            const math::vec2 block_origin = ui_text_renderer::block_origin_local(
                rect, layout, text.alignment, s);
            const float inv_size_x = 1.f / rect.size.x;
            const float inv_size_y = 1.f / rect.size.y;

            for (const auto& glyph : layout.glyphs)
            {
                const auto* slot = m_text_renderer.resolve(
                    font_res, glyph.raster_px, glyph.cp);
                if (slot->page == nullptr)
                    continue; // 无位图字形（布局阶段已过滤，防御）

                PrepareTextBatch(
                    chain, view,
                    slot->page->m_texture->resource(), shader_res);
                if (m_text_batch_vb == nullptr)
                    return; // 顶点缓冲池耗尽，丢弃剩余文本（已告警）

                // 字形盒（文本空间，y 向上）→ 矩形局部像素 → 单位矩形坐标
                //（MODEL 期望 ±0.5 张成元素矩形）→ 相机像素（行向量约定：
                // p' = p·M，平移位于 m[3][*]）。
                const float lx0 = block_origin.x + s * glyph.x;
                const float ly0 = block_origin.y + s * glyph.y;
                const float lx1 = lx0 + s * (float)slot->w;
                const float ly1 = ly0 + s * (float)slot->h;

                const float quad[4][2] = {
                    { lx0 * inv_size_x, ly0 * inv_size_y }, // 左下
                    { lx0 * inv_size_x, ly1 * inv_size_y }, // 左上
                    { lx1 * inv_size_x, ly0 * inv_size_y }, // 右下
                    { lx1 * inv_size_x, ly1 * inv_size_y }, // 右上
                };

                const float r = tint.x * glyph.color.x;
                const float g = tint.y * glyph.color.y;
                const float b = tint.z * glyph.color.z;
                const float a = tint.w * glyph.color.w;

                // 角点索引 0,1,2,2,1,3（与粒子系统同款四边形剖分）；
                // UV 与位置同角序（图集行序自底向上，v0 对应字形底边）。
                static constexpr size_t CORNER_UV[4][2] = {
                    { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } };

                float* v = &m_text_quad_data[
                    m_text_batch_quads * 4 * TEXT_VERTEX_FLOATS];
                ++m_text_batch_quads;

                for (size_t corner = 0; corner < 4; ++corner, v += TEXT_VERTEX_FLOATS)
                {
                    const float vx = quad[corner][0];
                    const float vy = quad[corner][1];

                    v[0] = vx * model[0][0] + vy * model[1][0] + model[3][0];
                    v[1] = vx * model[0][1] + vy * model[1][1] + model[3][1];
                    v[2] = 0.f;
                    v[3] = CORNER_UV[corner][0]
                        ? slot->u1 : slot->u0;
                    v[4] = CORNER_UV[corner][1]
                        ? slot->v1 : slot->v0;
                    v[5] = r;
                    v[6] = g;
                    v[7] = b;
                    v[8] = a;
                }
            }
        }

        // 批维护：键（图集页, 着色器）变化或当前缓冲写满时冲刷旧批、
        // 从池中取下一块顶点缓冲。
        void PrepareTextBatch(
            jegl_command_buffer* chain,
            const float(&view)[4][4],
            jegl_texture_2d* page,
            graphic::shader* shader_res)
        {
            if (m_text_batch_page == page && m_text_batch_shader == shader_res
                && m_text_batch_vb != nullptr
                && m_text_batch_quads < MAX_TEXT_QUADS_PER_VB)
                return;

            FlushTextBatch(chain, view);

            m_text_batch_page = page;
            m_text_batch_shader = shader_res;
            m_text_batch_vb = nullptr;

            if (m_text_vb_used >= MAX_TEXT_VB_COUNT)
            {
                if (!m_warned_text_vb_exhausted)
                {
                    m_warned_text_vb_exhausted = true;
                    jeecs::debug::logwarn(
                        "UserInterfaceGraphicPipelineSystem: text vertex buffer "
                        "pool (%zu buffers x %zu quads) exhausted, dropping "
                        "overflowing text runs this frame.",
                        MAX_TEXT_VB_COUNT, MAX_TEXT_QUADS_PER_VB);
                }
                return;
            }

            if (m_text_vb_pool.size() <= m_text_vb_used)
                m_text_vb_pool.push_back(CreateTextVertexBuffer());
            m_text_batch_vb = &m_text_vb_pool[m_text_vb_used++];
        }

        // 冲刷当前批：一次 jegl_update_vertex_buffer + 一次 command-buffer draw。
        // 顶点已烘焙至相机像素空间，M=I、MVP/MV=UI 视图矩阵。
        void FlushTextBatch(jegl_command_buffer* chain, const float(&view)[4][4])
        {
            if (m_text_batch_quads == 0 || m_text_batch_vb == nullptr)
            {
                m_text_batch_quads = 0;
                return;
            }

            auto& vb = *m_text_batch_vb;
            vb->update_buffer(
                m_text_quad_data.data(),
                m_text_batch_quads * 4 * TEXT_VERTEX_FLOATS * sizeof(float),
                m_text_batch_quads * 6);

            jegl_cmd_bind_texture_2d(chain, 0, m_text_batch_page);

            auto* pipeline = pipeline_for(
                m_text_batch_shader, vb->resource());
            render_draw_action draw_action(
                chain,
                pipeline,
                vb->resource(),
                (uint32_t)(m_text_batch_quads * 6),
                1);

            static const float MAT4_IDENTITY[4][4] = {
                {1.f, 0.f, 0.f, 0.f},
                {0.f, 1.f, 0.f, 0.f},
                {0.f, 0.f, 1.f, 0.f},
                {0.f, 0.f, 0.f, 1.f},
            };

            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_model, float4x4, MAT4_IDENTITY);
            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_model_view, float4x4, view);
            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_model_view_projection, float4x4, view);

            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_local_scale, float3, 1.f, 1.f, 1.f);
            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_uv_tiling, float2, 1.f, 1.f);
            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_uv_offset, float2, 0.f, 0.f);
            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_color, float4, 1.f, 1.f, 1.f, 1.f);

            m_text_batch_quads = 0;
        }

        // 创建一块文本动态顶点缓冲：静态角点索引一次分配，顶点数据逐帧
        // 整体重写并收缩激活索引数量（与粒子系统同款机制，见
        // jegl_update_vertex_buffer）。
        basic::resource<graphic::vertex> CreateTextVertexBuffer()
        {
            const size_t floats_per_vb =
                MAX_TEXT_QUADS_PER_VB * 4 * TEXT_VERTEX_FLOATS;

            // 首块缓冲顺带把共享发射暂存区扩到容量（零初始化同时作为
            // 缓冲创建期的初始数据）。
            if (m_text_quad_data.size() < floats_per_vb)
                m_text_quad_data.resize(floats_per_vb, 0.f);

            std::vector<uint32_t> indices;
            indices.reserve(MAX_TEXT_QUADS_PER_VB * 6);
            for (size_t q = 0; q < MAX_TEXT_QUADS_PER_VB; ++q)
            {
                const uint32_t base = (uint32_t)(q * 4);
                for (uint32_t idx : { base + 0, base + 1, base + 2,
                    base + 2, base + 1, base + 3 })
                    indices.push_back(idx);
            }

            auto created = graphic::vertex::create(
                jegl_vertex::TRIANGLES,
                m_text_quad_data.data(),
                m_text_quad_data.size() * sizeof(float),
                indices,
                {
                    { jegl_vertex::data_type::FLOAT32, 3 }, // vertex
                    { jegl_vertex::data_type::FLOAT32, 2 }, // uv
                    { jegl_vertex::data_type::FLOAT32, 4 }, // color
                });

            // 创建失败即显式失败（引擎资源创建约定，见 DefaultResources）。
            assert(created.has_value());
            return std::move(created.value());
        }
    };

    struct UnlitGraphicPipelineSystem : public BaseImpledGraphicPipeline
    {
        UnlitGraphicPipelineSystem(game_world w)
            : BaseImpledGraphicPipeline(w)
        {
        }

        ~UnlitGraphicPipelineSystem()
        {
        }

        void GraphicUpdate()
        {
            auto WINDOWS_SIZE = jeecs::input::windowsize();
            WINDOWS_WIDTH = (size_t)WINDOWS_SIZE.x;
            WINDOWS_HEIGHT = (size_t)WINDOWS_SIZE.y;

            m_camera_list.clear();
            m_renderer_list.clear();

            this->branch_allocate_begin();

            UnlitGraphicPipelineSystem::PrepareCameras();

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
                m_camera_list.emplace(
                    camera_arch{
                        branch, rendqueue, &projection, cameraviewport, rendbuf, frustumCulling, clear
                    });
            }

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
                    Element
                )
                >())
            {
                // RendOb will be input to a chain and used for swap
                m_renderer_list.emplace(
                    renderer_arch{
                        color, rendqueue, &trans, &shape, &shads, texs,
                        nullptr, nullptr, nullptr, nullptr, texarr
                    });
            }

            this->branch_allocate_end();
            DrawFrame();
        }

        void DrawFrame()
        {
            const double current_time = je_clock_time();

            math::vec4 shader_time =
            {
                (float)current_time,
                (float)abs(2.0 * (current_time * 2.0 - double(int(current_time * 2.0)) - 0.5)),
                (float)abs(2.0 * (current_time - double(int(current_time)) - 0.5)),
                (float)abs(2.0 * (current_time / 2.0 - double(int(current_time / 2.0)) - 0.5)) };

            for (auto& current_camera : m_camera_list)
            {
                graphic::framebuffer* rend_aim_buffer = nullptr;
                if (current_camera.rendToFramebuffer)
                {
                    if (current_camera.rendToFramebuffer->framebuffer.has_value())
                        rend_aim_buffer = current_camera.rendToFramebuffer->framebuffer->get();
                    else
                        continue;
                }

                const size_t
                    RENDAIMBUFFER_WIDTH = rend_aim_buffer ? rend_aim_buffer->width() : WINDOWS_WIDTH,
                    RENDAIMBUFFER_HEIGHT = rend_aim_buffer ? rend_aim_buffer->height() : WINDOWS_HEIGHT;

                const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
                const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;
                const float(&MAT4_VP)[4][4] = current_camera.projection->view_projection;

                update_default_uniform_buffer(
                    *current_camera.projection, MAT4_VIEW, MAT4_PROJECTION, MAT4_VP, shader_time);

                jegl_command_buffer* rend_chain = new_rend_chain_for_camera(
                    current_camera.branchPipeline, current_camera.viewport,
                    rend_aim_buffer, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                clear_color_if_any(rend_chain, current_camera.clear);
                jegl_cmd_clear_depth(rend_chain, 1.0);
                jegl_cmd_bind_uniform_buffer(rend_chain,
                    current_camera.projection->default_uniform_buffer->resource());

                draw_world_renderers(rend_chain, current_camera.frustumCulling, MAT4_VIEW, MAT4_VP);
            }
        }
    };

    struct DeferLight2DGraphicPipelineSystem : public BaseImpledGraphicPipeline
    {
        struct DeferLight2DResource
        {
            JECS_DISABLE_MOVE_AND_COPY(DeferLight2DResource);

            // Used for move rend result to camera's render aim buffer.
            jeecs::basic::resource<jeecs::graphic::texture2d> _no_shadow;
            jeecs::basic::resource<jeecs::graphic::vertex> _screen_vertex;

            jeecs::basic::resource<jeecs::graphic::vertex> _wall_shadow_vertex;

            // SpriteShadow（角色接触阴影）用单位四边形：顶点 ±1，
            // 顶点着色器把它映射到脚底椭圆 + 背光尾迹的包围框
            jeecs::basic::resource<jeecs::graphic::vertex> _sprite_shadow_quad;

            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_point_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_parallel_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_point_reverse_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_parallel_reverse_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_shape_point_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_shape_parallel_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sprite_point_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sprite_parallel_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_wall_point_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_wall_parallel_pass;

            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sub_pass;

            // 遮挡/自阴影 pass 写逐像素 painter key 深度，并以
            // ZTEST(LESS_EQUAL) / ZWRITE(ENABLE) 让更靠前的贡献胜出；
            // BLEND(MAX) 只负责同一 key 内的阴影叠加（不再用于跨层仲裁）。
            // 例外：SpriteShadow 的四边形只是椭圆/尾迹的包围框，片元里才
            // 裁出实际覆盖，因此它不写深度（ZWRITE DISABLE），只 MAX 叠加
            // 强度，避免包围框内无阴影的空洞以脚底 key 擦除其它阴影。

            // 数组纹理采样版 sub pass：瓦片地图分片把图集绑在
            // Textures2DArray 槽 0（顶点 uv 为 (u, v, 数组层号) float3），
            // 用本变体按层采样图集 alpha 做自阴影覆盖
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sub_array_pass;

            // 逐块判定版 sub pass ×2（TopDown 光源 + Light2D::PerBlockSelfShadow
            // 遮挡物专用）：AUTO 高度判定不在 CPU 按实体原点一刀切，而是用
            // 网格顶点属性 3 携带的块底边（局部 Y）在顶点着色器逐块比较，
            // 整块受光或整块自阴影。JE_COLOR 约定：x=factor，
            // y=高度线（AUTO 为光源世界 Y；ON 恒暗时为 +1e30），z/w 备用。
            // 仅分块大网格使用，同样按 MAX 合并（受光格不擦除遮挡阴影）
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sub_topdown_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sub_topdown_array_pass;

            inline static const float _screen_vertex_data[] = {
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
            inline static const float _wall_shadow_vertex_data[] = {
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
            inline static const float _sprite_shadow_quad_data[] = {
                -1.f, -1.f, 0.f,
                1.f, -1.f, 0.f,
                -1.f, 1.f, 0.f,
                1.f, 1.f, 0.f,
            };
            DeferLight2DResource()
                : _no_shadow{
                    jeecs::graphic::texture2d::create(1, 1, jegl_texture_common::format::RGBA)
                }
                , _screen_vertex{
                    jeecs::graphic::vertex::create(
                        jegl_vertex::type::TRIANGLESTRIP,
                        _screen_vertex_data,
                        sizeof(_screen_vertex_data),
                        {0, 1, 2, 3},
                        {
                            {jegl_vertex::data_type::FLOAT32, 3},
                            {jegl_vertex::data_type::FLOAT32, 2},
                        })
                        .value()
                }
                , _wall_shadow_vertex{
                    jeecs::graphic::vertex::create(
                        jegl_vertex::type::TRIANGLES,
                        _wall_shadow_vertex_data,
                        sizeof(_wall_shadow_vertex_data),
                        {0, 1, 2, 1, 3, 2,
                        4, 5, 6, 5, 7, 6,
                        8, 9, 10, 9, 11, 10,
                        12, 13, 14, 13, 15, 14},
                        {
                            {jegl_vertex::data_type::FLOAT32, 3},
                            {jegl_vertex::data_type::FLOAT32, 2},
                            {jegl_vertex::data_type::FLOAT32, 1},
                            {jegl_vertex::data_type::FLOAT32, 2},
                        })
                        .value()
                }
                , _sprite_shadow_quad{
                    jeecs::graphic::vertex::create(
                        jegl_vertex::type::TRIANGLESTRIP,
                        _sprite_shadow_quad_data,
                        sizeof(_sprite_shadow_quad_data),
                        {0, 1, 2, 3},
                        {
                            {jegl_vertex::data_type::FLOAT32, 3},
                        })
                        .value()
                }
                , _defer_light2d_shadow_point_pass{
                    jeecs::graphic::shader::load(nullptr, "!/builtin/shader/internal/defer_light2d_shadow_point.shader").value()
                }
                , _defer_light2d_shadow_parallel_pass{
                    jeecs::graphic::shader::load(nullptr, "!/builtin/shader/internal/defer_light2d_shadow_parallel.shader").value()
                }
                , _defer_light2d_shadow_point_reverse_pass{
                jeecs::graphic::shader::load(nullptr, "!/builtin/shader/internal/defer_light2d_shadow_reverse_point.shader").value()
                }
                , _defer_light2d_shadow_parallel_reverse_pass{
                    jeecs::graphic::shader::load(nullptr, "!/builtin/shader/internal/defer_light2d_shadow_reverse_parallel.shader").value()
                }
                , _defer_light2d_shadow_shape_point_pass{
                    jeecs::graphic::shader::load(nullptr, "!/builtin/shader/internal/defer_light2d_shadow_point_shape.shader").value()
                }
                , _defer_light2d_shadow_shape_parallel_pass{
                    jeecs::graphic::shader::load(nullptr, "!/builtin/shader/internal/defer_light2d_shadow_parallel_shape.shader").value()
                }
                , _defer_light2d_shadow_sprite_point_pass{
                    jeecs::graphic::shader::load(nullptr, "!/builtin/shader/internal/defer_light2d_shadow_point_sprite.shader").value()
                }
                , _defer_light2d_shadow_sprite_parallel_pass{
                jeecs::graphic::shader::load(nullptr, "!/builtin/shader/internal/defer_light2d_shadow_parallel_sprite.shader").value()
                }
                , _defer_light2d_shadow_wall_point_pass{
                    jeecs::graphic::shader::load(nullptr, "!/builtin/shader/internal/defer_light2d_shadow_point_wall.shader").value()
                }
                , _defer_light2d_shadow_wall_parallel_pass{
                jeecs::graphic::shader::load(nullptr, "!/builtin/shader/internal/defer_light2d_shadow_parallel_wall.shader").value()
                }
                , _defer_light2d_shadow_sub_pass{
                    jeecs::graphic::shader::load(nullptr, "!/builtin/shader/internal/defer_light2d_shadow_sub.shader").value()
                }
                , _defer_light2d_shadow_sub_array_pass{
                    jeecs::graphic::shader::load(nullptr, "!/builtin/shader/internal/defer_light2d_shadow_sub_array.shader").value()
                }
                , _defer_light2d_shadow_sub_topdown_pass{
                    jeecs::graphic::shader::load(nullptr, "!/builtin/shader/internal/defer_light2d_shadow_sub_topdown.shader").value()
                }
                , _defer_light2d_shadow_sub_topdown_array_pass{
                    jeecs::graphic::shader::load(nullptr, "!/builtin/shader/internal/defer_light2d_shadow_sub_topdown_array.shader").value()
                }
            {
                // Donothing...
            }
        };

        DeferLight2DResource m_defer_light2d_host;

        struct l2dcamera_arch
        {
            je_RenderBranch* branchPipeline;

            const Rendqueue* rendqueue;
            const Translation* translation;
            const Projection* projection;
            const Viewport* viewport;
            const RendToFramebuffer* rendToFramebuffer;
            const Light2D::CameraPostPass* light2DPostPass;
            const Shaders* shaders;
            const Textures2D* textures;
            const FrustumCulling* frustumCulling;
            const Camera::Clear* clear;
            const Renderer::Color* color;

            bool operator<(const l2dcamera_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };

        struct light2d_arch
        {
            const Translation* translation;
            const Light2D::TopDown* topdown;
            const Light2D::Point* point;
            const Light2D::Parallel* parallel;
            const Light2D::Range* range;
            // 阴影计算点偏移（可选）：阴影投射与高度判定改用
            // world_position + world_rotation * offset 为原点；
            // 光照本身仍从 translation->world_position 渲染
            const Light2D::ShadowOffset* shadowoffset;
            const Light2D::Gain* gain;
            const Light2D::ShadowBuffer* shadowbuffer;
            const Renderer::Color* color;
            const Shape* shape;
            const Shaders* shaders;
            const Textures2D* textures;
        };

        struct block2d_arch
        {
            const Translation* translation;
            // 实体世界包围盒底边中点（.y 即底部 Y）：构建列表时按形状局部
            // AABB 八角点变换计算一次，供 TopDown 排序 / 分层、各阴影的
            // auto_disable 判定与 SpriteShadow 椭圆中心复用
            math::vec3 bottom_world_center;
            // 同一包围盒的顶边中点（.y 即顶部 Y）：仅用于 TopDown 阴影
            // 逐像素深度映射的 key 范围，保证逐块/逐边 key 都落在范围内
            math::vec3 top_world_center;

            const Light2D::BlockShadow* blockshadow;
            const Light2D::ShapeShadow* shapeshadow;
            const Light2D::SpriteShadow* spriteshadow;
            const Light2D::WallShadow* wallshadow;
            const Light2D::SelfShadow* selfshadow;
            // 逐块自阴影修饰（与 SelfShadow 同挂）：TopDown 光源下 AUTO
            // 高度判定用网格顶点携带的块底边逐块比较，见 sub_topdown pass
            const Light2D::PerBlockSelfShadow* perblock;

            const Textures2D* textures;
            // 数组图集（瓦片地图分片）：槽 0 为图集，sub pass 用它按层采样
            const Textures2DArray* textures_array;
            const Shape* shape;
        };

        std::multiset<l2dcamera_arch> m_2dcamera_list;
        std::vector<light2d_arch> m_2dlight_list;

        std::vector<block2d_arch> m_2dblock_z_list;
        std::vector<block2d_arch> m_2dblock_y_list;

        DeferLight2DGraphicPipelineSystem(game_world w)
            : BaseImpledGraphicPipeline(w)
        {
        }

        ~DeferLight2DGraphicPipelineSystem()
        {
        }

        void GraphicUpdate()
        {
            auto WINDOWS_SIZE = jeecs::input::windowsize();
            WINDOWS_WIDTH = (size_t)WINDOWS_SIZE.x;
            WINDOWS_HEIGHT = (size_t)WINDOWS_SIZE.y;

            m_2dlight_list.clear();
            m_2dblock_z_list.clear();
            m_2dcamera_list.clear();
            m_renderer_list.clear();

            this->branch_allocate_begin();

            DeferLight2DGraphicPipelineSystem::PrepareCameras();

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
                m_2dcamera_list.emplace(
                    l2dcamera_arch{
                        branch,
                        rendqueue,
                        &tarns,
                        &projection,
                        cameraviewport,
                        rendbuf,
                        light2dpostpass,
                        shaders,
                        textures,
                        frustumCulling,
                        clear,
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
                            (rend_aim_buffer ? rend_aim_buffer->width() : WINDOWS_WIDTH)));
                    const size_t RENDAIMBUFFER_HEIGHT =
                        std::max((size_t)1, (size_t)llround(
                            (cameraviewport ? cameraviewport->viewport.w : 1.0f) *
                            (rend_aim_buffer ? rend_aim_buffer->height() : WINDOWS_HEIGHT)));

                    const size_t LIGHT_BUFFER_WIDTH =
                        std::max((size_t)1, (size_t)llround(
                            RENDAIMBUFFER_WIDTH * std::max(0.f, std::min(light2dpostpass->light_rend_ratio, 1.0f))));

                    const size_t LIGHT_BUFFER_HEIGHT =
                        std::max((size_t)1, (size_t)llround(
                            RENDAIMBUFFER_HEIGHT * std::max(0.f, std::min(light2dpostpass->light_rend_ratio, 1.0f))));

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
                                    jegl_texture_common::format::RGBA,
                                    // 自发光颜色，用于法线反射或者发光物体的颜色参数，最终混合shader会将此参数用于光照计算
                                    jegl_texture_common::format(jegl_texture_common::format::RGBA | jegl_texture_common::format::FLOAT16),
                                    // 视空间坐标(RGB) Alpha通道暂时留空
                                    jegl_texture_common::format(jegl_texture_common::format::RGBA | jegl_texture_common::format::FLOAT16),
                                    // 法线空间颜色
                                    jegl_texture_common::format(jegl_texture_common::format::RGBA | jegl_texture_common::format::FLOAT16),
                                },
                                true).value();
                        light2dpostpass->post_light_target
                            = jeecs::graphic::framebuffer::create(
                                LIGHT_BUFFER_WIDTH,
                                LIGHT_BUFFER_HEIGHT,
                                {
                                    // 光渲染结果
                                    (jegl_texture_common::format)(jegl_texture_common::format::RGBA | jegl_texture_common::format::FLOAT16),
                                },
                                false).value();
                    }
                }
            }

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
                // RendOb will be input to a chain and used for swap
                m_renderer_list.emplace(
                    renderer_arch{
                        color, rendqueue, &trans, &shape, &shads, texs,
                        nullptr, nullptr, nullptr, nullptr, texarr
                    });
            }

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
                m_2dlight_list.emplace_back(
                    light2d_arch{
                        &trans, topdown, point, parallel, range, shadowoffset, gain, shadowbuffer,
                        color, &shape, &shads, texs,
                    });
                if (shadowbuffer != nullptr)
                {
                    size_t SHADOW_BUFFER_WIDTH =
                        std::max((size_t)1, (size_t)llround(
                            WINDOWS_WIDTH * std::max(0.f, std::min(shadowbuffer->resolution_ratio, 1.0f))));

                    size_t SHADOW_BUFFER_HEIGHT =
                        std::max((size_t)1, (size_t)llround(
                            WINDOWS_HEIGHT * std::max(0.f, std::min(shadowbuffer->resolution_ratio, 1.0f))));

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
                            std::max((size_t)1, SHADOW_BUFFER_WIDTH),
                            std::max((size_t)1, SHADOW_BUFFER_HEIGHT),
                            {
                                jegl_texture_common::format::RGBA,
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
                            jegl_vertex::type::TRIANGLESTRIP,
                            vertex_datas.data(), vertex_datas.size() * sizeof(float),
                            index_datas,
                            {
                                {jegl_vertex::data_type::FLOAT32, 3},
                                {jegl_vertex::data_type::FLOAT32, 1},
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
                                jegl_vertex::type::TRIANGLESTRIP,
                                _vertex_buffer.data(), _vertex_buffer.size() * sizeof(float),
                                _index_buffer,
                                {
                                    {jegl_vertex::data_type::FLOAT32, 3},
                                    {jegl_vertex::data_type::FLOAT32, 1},
                                    {jegl_vertex::data_type::FLOAT32, 1},
                                });
                        }
                        else
                            blockshadow->shape.m_block_mesh.reset();
                    }
                }
                math::vec3 top_world_center;
                m_2dblock_z_list.push_back(
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

            m_2dblock_y_list = m_2dblock_z_list;
            std::sort(m_2dblock_z_list.begin(), m_2dblock_z_list.end(),
                [](const block2d_arch& a, const block2d_arch& b)
                {
                    return a.translation->world_position.z > b.translation->world_position.z;
                });
            // TopDown 光源遍历用：按实体底部 Y 降序（高大实体也以脚下位置
            // 参与层级排序，而不是原点中心）。底部 Y 相同时按 z 降序平手——
            // z 小者（默认沿 -z 叠放的视觉上层）后 flush、其自阴影覆盖在
            // 共享像素生效，与普通光源按 z 降序遍历的层级结果一致，避免
            // 不稳定排序导致覆盖胜负不确定。
            // 注意：这里只决定遍历/分层顺序；最终逐像素胜负由 shadow buffer
            // 的深度测试（JE_SHADOW_KEY）决定，chunk 级排序键不再能覆盖
            // 更靠前的 SelfShadow。逐段/逐块 key 见各 shadow shader。
            std::sort(m_2dblock_y_list.begin(), m_2dblock_y_list.end(),
                [](const block2d_arch& a, const block2d_arch& b)
                {
                    const float ay = a.bottom_world_center.y;
                    const float by = b.bottom_world_center.y;
                    return ay > by || (ay == by
                        && a.translation->world_position.z > b.translation->world_position.z);
                });

            this->branch_allocate_end();
            DrawFrame();
        }

        /* ------- DeferLight2D-specific helpers (use the base helpers too) ------- */

        // Pick the shadow shader set based on light type (point/parallel).
        struct defer_light2d_shadow_passes_t
        {
            const basic::resource<graphic::shader>& normal;
            const basic::resource<graphic::shader>& reverse_normal;
            const basic::resource<graphic::shader>& shape;
            const basic::resource<graphic::shader>& sprite;
            const basic::resource<graphic::shader>& wall;
            const basic::resource<graphic::shader>& sub;
            const basic::resource<graphic::shader>& sub_texarray;
        };
        defer_light2d_shadow_passes_t select_shadow_passes(bool is_parallel) const
        {
            return is_parallel
                ? defer_light2d_shadow_passes_t{
                    m_defer_light2d_host._defer_light2d_shadow_parallel_pass,
                    m_defer_light2d_host._defer_light2d_shadow_parallel_reverse_pass,
                    m_defer_light2d_host._defer_light2d_shadow_shape_parallel_pass,
                    m_defer_light2d_host._defer_light2d_shadow_sprite_parallel_pass,
                    m_defer_light2d_host._defer_light2d_shadow_wall_parallel_pass,
                    m_defer_light2d_host._defer_light2d_shadow_sub_pass,
                    m_defer_light2d_host._defer_light2d_shadow_sub_array_pass,
            }
            : defer_light2d_shadow_passes_t{
                m_defer_light2d_host._defer_light2d_shadow_point_pass,
                m_defer_light2d_host._defer_light2d_shadow_point_reverse_pass,
                m_defer_light2d_host._defer_light2d_shadow_shape_point_pass,
                m_defer_light2d_host._defer_light2d_shadow_sprite_point_pass,
                m_defer_light2d_host._defer_light2d_shadow_wall_point_pass,
                m_defer_light2d_host._defer_light2d_shadow_sub_pass,
                m_defer_light2d_host._defer_light2d_shadow_sub_array_pass,
            };
        }

        void bind_shadow_texture(
            jegl_command_buffer* chain, /* OPTIONAL */ const Textures2D* textures)
        {
            auto main_texture =
                textures == nullptr ? std::nullopt : textures->get_texture(0);

            jegl_cmd_bind_texture_2d(
                chain, 0,
                main_texture.has_value()
                ? main_texture.value()->resource()
                : m_default_resources.default_texture->resource());
        }

        // 实体世界包围盒底边中点：取形状局部 AABB 的 8 个角点经
        // object2world 变换后，X/Z 取中值、Y 取最小值。兼容旋转与非居中
        // 轴心——实体旋转后该点仍在世界空间底部（脚下）。
        // 结果缓存进 block2d_arch::bottom_world_center；TopDown 排序/分层、
        // 各阴影的 auto_disable 与 SpriteShadow 椭圆中心均复用它。
        // top_out 返回同一 AABB 的顶边中点（.y 为最大值），用于 TopDown
        // 阴影深度映射的 key 上界（逐块/逐边 key 可能高于实体底边）。
        // 挂 PerBlockSelfShadow 的分块大网格走顶点属性逐块判定
        //（见 sub_topdown pass），不经过这里。
        math::vec3 get_block_bottom_world_center(
            const Translation& translation, const Shape& shape,
            /* OPTIONAL */ math::vec3* top_out = nullptr) const
        {
            const auto& using_shape = shape.vertex.has_value()
                ? shape.vertex.value()
                : m_default_resources.default_shape_quad;
            const auto* vertex_data = using_shape->resource();
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

        // 阴影逐像素排序的 key→NDC 深度线性映射：TopDown 用世界 Y，
        // 普通 2D 用世界 Z；key 越小越靠前，映射到 [0.05, 0.95] 后由
        // ZTEST(LESS_EQUAL) 保留最靠前的阴影贡献。
        struct shadow_depth_mapping
        {
            float scale = 1.f;
            float bias = 0.f;
        };
        shadow_depth_mapping make_shadow_depth_mapping(
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

        // Unified draw call for the 3 Light2D shadow occluder types (block / shape / sprite).
        // They all compute MV/MVP from the occluder's object2world and pack shadow params
        // into local_scale + color. Block uses (0,-1,1).unit() as parallel light dir ref;
        // shape/sprite use (0,-1,0). shadow_cast_pos is the point-light shadow origin
        //（world_position + world_rotation * ShadowOffset，见 DrawFrame）.
        void draw_shadow_occluder(
            jegl_command_buffer* shadow_chain,
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
            /* OPTIONAL */ const math::vec4* color_override = nullptr,
            /* OPTIONAL */ const math::vec4* ellipse_params = nullptr)
        {
            auto* vertex_resource = shape_vertex->resource();
            auto* pipeline = pipeline_for(
                shadow_shader.get(), vertex_resource);

            render_draw_action draw_action(
                shadow_chain,
                pipeline,
                vertex_resource,
                (uint32_t)vertex_resource->m_index_count,
                1);

            const float(&MAT4_MODEL)[4][4] = occluder_translation->object2world;
            float MAT4_MVP[4][4], MAT4_MV[4][4];
            math::mat4xmat4(MAT4_MVP, vp, MAT4_MODEL);
            math::mat4xmat4(MAT4_MV, view, MAT4_MODEL);

            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_model, float4x4, MAT4_MODEL);
            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_model_view, float4x4, MAT4_MV);
            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_model_view_projection, float4x4, MAT4_MVP);
            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_local_scale, float3,
                local_scale.x, local_scale.y, local_scale.z);
            // 阴影逐像素排序键：x=实体/顶点 key，y/z=深度映射，w=模式位
            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_shadow_key, float4,
                shadow_key.x, shadow_key.y, shadow_key.z, shadow_key.w);

            if (textures != nullptr)
            {
                JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_uv_tiling, float2,
                    textures->tiling.x, textures->tiling.y);
                JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_uv_offset, float2,
                    textures->offset.x, textures->offset.y);
            }

            // 通过 je_color 变量传递阴影计算点（点光，含 ShadowOffset 偏移）
            // 或方向（平行光）；SpriteShadow 用 color_override 改为传脚底
            // 椭圆中心（世界空间 XY + 中心世界 Z 在 .w）
            if (color_override != nullptr)
            {
                JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_color, float4,
                    color_override->x,
                    color_override->y,
                    color_override->z,
                    color_override->w);
            }
            else if (light.parallel != nullptr)
            {
                math::vec3 rotated_light_dir = light.translation->world_rotation * parallel_dir_ref;
                JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_color, float4,
                    rotated_light_dir.x, rotated_light_dir.y, rotated_light_dir.z, color_w);
            }
            else
            {
                JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_color, float4,
                    shadow_cast_pos.x,
                    shadow_cast_pos.y,
                    shadow_cast_pos.z,
                    color_w);
            }

            // SpriteShadow 接触椭圆参数（其余遮挡物不使用）
            if (ellipse_params != nullptr)
            {
                JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_shadow_ellipse, float4,
                    ellipse_params->x,
                    ellipse_params->y,
                    ellipse_params->z,
                    ellipse_params->w);
            }
        }

        void DrawFrame()
        {
            const double current_time = je_clock_time();

            math::vec4 shader_time =
            {
                (float)current_time,
                (float)abs(2.0 * (current_time * 2.0 - double(int(current_time * 2.0)) - 0.5)),
                (float)abs(2.0 * (current_time - double(int(current_time)) - 0.5)),
                (float)abs(2.0 * (current_time / 2.0 - double(int(current_time / 2.0)) - 0.5)) };

            // 阴影逐像素排序：每帧按当前 block 列表计算一次 key→深度映射。
            // TopDown 用世界 Y，普通 2D 用世界 Z；两种模式各一份。
            const shadow_depth_mapping shadow_depth_z =
                make_shadow_depth_mapping(m_2dblock_z_list, false);
            const shadow_depth_mapping shadow_depth_y =
                make_shadow_depth_mapping(m_2dblock_y_list, true);

            for (auto& current_camera : m_2dcamera_list)
            {
                graphic::framebuffer* rend_aim_buffer = nullptr;
                if (current_camera.rendToFramebuffer)
                {
                    if (current_camera.rendToFramebuffer->framebuffer.has_value())
                        rend_aim_buffer = current_camera.rendToFramebuffer->framebuffer->get();
                    else
                        continue;
                }

                const size_t
                    RENDAIMBUFFER_WIDTH = rend_aim_buffer ? rend_aim_buffer->width() : WINDOWS_WIDTH,
                    RENDAIMBUFFER_HEIGHT = rend_aim_buffer ? rend_aim_buffer->height() : WINDOWS_HEIGHT;

                const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
                const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;
                const float(&MAT4_VP)[4][4] = current_camera.projection->view_projection;

                update_default_uniform_buffer(
                    *current_camera.projection, MAT4_VIEW, MAT4_PROJECTION, MAT4_VP, shader_time);

                jegl_command_buffer* rend_chain = nullptr;

                std::vector<light2d_arch*> _2dlight_after_culling;
                _2dlight_after_culling.reserve(m_2dlight_list.size());

                // If current camera contain light2d-pass, prepare light shadow here.
                if (current_camera.light2DPostPass != nullptr)
                {
                    if (!current_camera.light2DPostPass->post_rend_target.has_value()
                        || !current_camera.light2DPostPass->post_light_target.has_value())
                        // Not ready, skip this frame.
                        continue;

                    // Walk throw all light, rend shadows to light's ShadowBuffer.
                    for (auto& lightarch : m_2dlight_list)
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
                            jegl_command_buffer* light2d_shadow_rend_chain = new_rend_chain_for_camera(
                                current_camera.branchPipeline,
                                /*viewport=*/nullptr,
                                light2d_shadow_aim_buffer.get(),
                                light2d_shadow_aim_buffer->width(),
                                light2d_shadow_aim_buffer->height());

                            const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
                            jegl_cmd_clear_color(light2d_shadow_rend_chain, 0, clear_color);
                            // 深度清为 1（远）；shadow shader 把 painter key 映射
                            // 到 [0.05, 0.95]，LESS_EQUAL 保留最小（最靠前）值
                            jegl_cmd_clear_depth(light2d_shadow_rend_chain, 1.0);

                            jegl_cmd_bind_uniform_buffer(light2d_shadow_rend_chain,
                                current_camera.projection->default_uniform_buffer->resource());

                            const auto shadow_passes = select_shadow_passes(lightarch.parallel != nullptr);

                            const bool topdown_light = lightarch.topdown != nullptr;
                            const shadow_depth_mapping& shadow_depth =
                                topdown_light ? shadow_depth_y : shadow_depth_z;
                            // 统一 key 模式下 TopDown=1 / 普通=0；逐顶点 key
                            // 由 +2 位标记（0/1 统一，2/3 逐顶点）
                            const float shadow_key_base_mode = topdown_light ? 1.f : 0.f;

                            std::list<block2d_arch*> block_in_current_layer;

                            auto block2d_iter = lightarch.topdown == nullptr ? m_2dblock_z_list.begin() : m_2dblock_y_list.begin();
                            auto block2d_end = lightarch.topdown == nullptr ? m_2dblock_z_list.end() : m_2dblock_y_list.end();

                            for (; block2d_iter != block2d_end; ++block2d_iter)
                            {
                                auto& blockarch = *block2d_iter;

                                int64_t current_layer = lightarch.topdown == nullptr
                                    ? (int64_t)(blockarch.translation->world_position.z * 100.f)
                                    : (int64_t)(blockarch.bottom_world_center.y * 100.f);

                                const float block_range = 0.5f *
                                    get_entity_size(*blockarch.translation, blockarch.shape->vertex).length();

                                const auto l2b_distance = (math::vec2(blockarch.translation->world_position) -
                                    math::vec2(shadow_cast_pos))
                                    .length();

                                if (lightarch.parallel != nullptr || l2b_distance <= block_range + light_range)
                                {
                                    block_in_current_layer.push_back(&blockarch);

                                    // 实体级 painter key：TopDown=包围盒底边世界 Y，
                                    // 普通 2D=实体世界 Z。逐段 key（瓦片轮廓）在
                                    // 顶点着色器里用顶点世界坐标覆盖它。
                                    const float shadow_entity_key = topdown_light
                                        ? blockarch.bottom_world_center.y
                                        : blockarch.translation->world_position.z;
                                    const math::vec4 shadow_key(
                                        shadow_entity_key,
                                        shadow_depth.scale,
                                        shadow_depth.bias,
                                        shadow_key_base_mode);
                                    const float block_shadow_mode = shadow_key_base_mode
                                        + (blockarch.blockshadow != nullptr
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
                                        // BlockShadow 用 (0,-1,1).unit() 作为平行光方向参考，
                                        // 通过 local_scale.x 传递阴影权重，color.w 固定为 1。
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
                                            math::vec3(blockarch.blockshadow->factor, 0.f, 0.f),
                                            /*color_w=*/1.f,
                                            math::vec3(0.f, -1.f, 1.f).unit(),
                                            blockarch.textures,
                                            block_shadow_key);
                                    }
                                    if (blockarch.shapeshadow != nullptr
                                        && blockarch.shapeshadow->factor > 0.f
                                        && (light_is_above_block || !blockarch.shapeshadow->auto_disable))
                                    {
                                        assert(blockarch.shape != nullptr);
                                        const auto& using_shape = blockarch.shape->vertex.has_value()
                                            ? blockarch.shape->vertex.value()
                                            : m_default_resources.default_shape_quad;
                                        bind_shadow_texture(
                                            light2d_shadow_rend_chain, blockarch.textures);

                                        // ShapeShadow 通过 local_scale.x 传权重，.yz 传 tiling_scale，
                                        // color.w 传 distance，平行光方向参考 (0,-1,0)。
                                        draw_shadow_occluder(
                                            light2d_shadow_rend_chain,
                                            shadow_passes.shape,
                                            using_shape,
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
                                            m_defer_light2d_host._wall_shadow_vertex,
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
                                    {
                                        // SpriteShadow（角色接触阴影）：在实体世界包围盒
                                        // 底边中点（脚下）画椭圆，并沿背光方向拉出尾迹。
                                        // 中心取世界空间底部，旋转实体不会带着椭圆一起
                                        // 转离地面。
                                        // local_scale.x 传强度，.yz 传 CPU 预计算的世界
                                        // 空间背光方向（着色器用 JE_V 转到视空间，避免
                                        // 实体非等比缩放扭曲方向）；JE_COLOR.xy 传椭圆
                                        // 中心（世界空间），.z 传阴影计算点到实体的平面
                                        // 距离（点光用于切线张开率，平行光不用），.w 传
                                        // 中心世界 Z；JE_SHADOW_ELLIPSE 传轴长/延伸/消隐
                                        // 距离。点光方向取"阴影计算点→脚底"（而非
                                        // →实体中心），尾迹从接触点向外延伸；平行光方向
                                        // 由光源自身朝向决定，与实体位置无关
                                        const math::vec3 shadow_dir_world_raw =
                                            (lightarch.parallel != nullptr
                                                ? lightarch.translation->world_rotation * math::vec3(0.f, -1.f, 0.f)
                                                : blockarch.bottom_world_center
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

                                        assert(blockarch.shape != nullptr);
                                        const math::vec3 ellipse_center_world =
                                            blockarch.bottom_world_center;

                                        // 点光阴影由着色器从阴影计算点向椭圆作切线锥，
                                        // 需要椭圆中心到阴影计算点的平面距离（JE_COLOR.z）；
                                        // 平行光不用，写 0。距离取世界空间椭圆中心
                                        // 到阴影计算点的 XY 距离。
                                        float light_plane_distance = 0.f;
                                        if (lightarch.parallel == nullptr)
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

                                        const float shadow_distance = std::max(
                                            blockarch.spriteshadow->distance, 0.f);
                                        const float shadow_fade = blockarch.spriteshadow->fade_distance > 0.f
                                            ? std::min(blockarch.spriteshadow->fade_distance, shadow_distance)
                                            : shadow_distance;
                                        const math::vec4 ellipse_params(
                                            blockarch.spriteshadow->ellipse_major,
                                            blockarch.spriteshadow->ellipse_minor,
                                            shadow_distance,
                                            shadow_fade);

                                        draw_shadow_occluder(
                                            light2d_shadow_rend_chain,
                                            shadow_passes.sprite,
                                            m_defer_light2d_host._sprite_shadow_quad,
                                            blockarch.translation,
                                            lightarch,
                                            shadow_cast_pos,
                                            MAT4_VIEW, MAT4_VP,
                                            math::vec3(
                                                blockarch.spriteshadow->factor,
                                                shadow_dir_world.x,
                                                shadow_dir_world.y),
                                            0.f,
                                            math::vec3(0.f, -1.f, 0.f),
                                            nullptr,
                                            shadow_key,
                                            &ellipse_center,
                                            &ellipse_params);
                                    }
                                }

                                // 如果下一个阴影将会在不同层级，或者当前阴影是最后一个阴影，则更新阴影覆盖
                                auto next_block2d_iter = block2d_iter + 1;
                                if (next_block2d_iter == block2d_end || current_layer != (int64_t)((lightarch.topdown == nullptr
                                    ? next_block2d_iter->translation->world_position.z
                                    : next_block2d_iter->bottom_world_center.y) *
                                    100.f))
                                {
                                    for (auto* block_in_layer : block_in_current_layer)
                                    {
                                        // 如果物体被指定为不需要cover，那么就不绘制
                                        if (block_in_layer->selfshadow != nullptr)
                                        {
                                            // 瓦片地图分片的图集绑在数组纹理槽 0
                                            //（顶点 uv.z 为数组层号），走数组采样版
                                            // sub pass；其余实体按普通 2D 纹理采样。
                                            // 数组绑定在 2D 绑定之后写入槽 0（后写覆盖）
                                            std::optional<basic::resource<graphic::texture_array>> array_atlas;
                                            if (block_in_layer->textures_array != nullptr)
                                                array_atlas = block_in_layer->textures_array->get_texture(0);

                                            bind_shadow_texture(
                                                light2d_shadow_rend_chain, block_in_layer->textures);
                                            if (array_atlas.has_value())
                                                jegl_cmd_bind_texture_array(
                                                    light2d_shadow_rend_chain, 0,
                                                    array_atlas.value()->resource());

                                            // TopDown 光源 + PerBlockSelfShadow 修饰：
                                            // 换用按块底边逐块判定的 topdown sub pass
                                            const bool per_block_selfshadow =
                                                lightarch.topdown != nullptr
                                                && block_in_layer->perblock != nullptr;

                                            // 非 TopDown 的自阴影（普通精灵与瓦片分片
                                            // 同款）用覆盖语义：写入即替换阴影缓冲，
                                            // 由 painter key 深度决定谁能覆盖谁。
                                            // 自阴影 key 取实体/图层 key 减半层偏移，
                                            // 因此本层 BlockShadow 落在本层图块上的
                                            // 自遮挡会被清除（同一 key 的遮挡阴影被
                                            // 自阴影压过后写入），而更靠前图层/遮挡体
                                            // 的阴影 key 更小、仍通过深度测试保留。
                                            // TopDown 的逐块 key 与同层遮挡段的 key
                                            // 不可直接比较，保留 MAX 合并语义。
                                            auto& sub_pass_shader = per_block_selfshadow
                                                ? (array_atlas.has_value()
                                                    ? m_defer_light2d_host._defer_light2d_shadow_sub_topdown_array_pass
                                                    : m_defer_light2d_host._defer_light2d_shadow_sub_topdown_pass)
                                                : (array_atlas.has_value()
                                                    ? shadow_passes.sub_texarray
                                                    : shadow_passes.sub);

                                            assert(block_in_layer->shape != nullptr);
                                            const auto& using_shape =
                                                block_in_layer->shape->vertex.has_value()
                                                ? block_in_layer->shape->vertex.value()
                                                : m_default_resources.default_shape_quad;

                                            auto* vertex_resource = using_shape->resource();
                                            auto* pipeline = pipeline_for(
                                                sub_pass_shader.get(), vertex_resource);

                                            render_draw_action draw_action(
                                                light2d_shadow_rend_chain,
                                                pipeline,
                                                vertex_resource,
                                                (uint32_t)vertex_resource->m_index_count,
                                                1);

                                            const float(&MAT4_MODEL)[4][4] = block_in_layer->translation->object2world;
                                            float MAT4_MVP[4][4], MAT4_MV[4][4];
                                            math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                                            math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                                            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_model, float4x4, MAT4_MODEL);
                                            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_model_view, float4x4, MAT4_MV);
                                            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_model_view_projection, float4x4, MAT4_MVP);

                                            // 自阴影参与逐像素排序：
                                            //  * 普通 SelfShadow 与瓦片分片都用实体/
                                            //    图层 key 减半层（0.005）：同层内覆盖
                                            //    BlockShadow（消除瓦片自遮挡），更靠前
                                            //    的遮挡阴影仍然保留；
                                            //  * TopDown 的 PerBlockSelfShadow 由顶点
                                            //    着色器用格顶（bottom + 1）逐块生成 key。
                                            const float self_entity_key = topdown_light
                                                ? block_in_layer->bottom_world_center.y
                                                : block_in_layer->translation->world_position.z;
                                            float self_shadow_key_value = self_entity_key;
                                            float self_shadow_mode = shadow_key_base_mode;
                                            if (per_block_selfshadow)
                                                self_shadow_mode = 3.f;
                                            else
                                                self_shadow_key_value -= 0.005f;

                                            JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_shadow_key, float4,
                                                self_shadow_key_value,
                                                shadow_depth.scale,
                                                shadow_depth.bias,
                                                self_shadow_mode);

                                            // local_scale.x .y .z 通道预留
                                            /*NEED_AND_SET_UNIFORM(local_scale, float3,
                                                0.f,
                                                0.f,
                                                0.f);*/

                                            if (per_block_selfshadow)
                                            {
                                                // 高度线打包进 JE_COLOR 供顶点着色器逐块比较：
                                                // AUTO 用阴影计算点世界 Y；ON（auto_disable=false）
                                                // 恒暗，高度线取 +1e30 使 lit 恒 0（整块写 factor）
                                                const float height_line =
                                                    block_in_layer->selfshadow->auto_disable
                                                    ? shadow_cast_pos.y
                                                    : 1e30f;
                                                JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_color, float4,
                                                    block_in_layer->selfshadow->factor,
                                                    height_line,
                                                    0.f,
                                                    0.f);
                                            }
                                            // TopDown 光源下 AUTO 以实体底边（包围盒
                                            // 世界 Y 最小值）判定，避免高大实体以原点
                                            // 高于光源时被整块点亮；普通光源仍按原点 Z。
                                            else if (block_in_layer->selfshadow->auto_disable &&
                                                (lightarch.topdown == nullptr
                                                    ? block_in_layer->translation->world_position.z >= shadow_cast_pos.z
                                                    : block_in_layer->bottom_world_center.y >= shadow_cast_pos.y))
                                                JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_color, float4, 0.f, 0.f, 0.f, 0.f);
                                            else
                                                JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_color, float4,
                                                    block_in_layer->selfshadow->factor,
                                                    block_in_layer->selfshadow->factor,
                                                    block_in_layer->selfshadow->factor,
                                                    block_in_layer->selfshadow->factor);

                                            if (block_in_layer->textures != nullptr)
                                            {
                                                JE_CHECK_NEED_AND_SET_UNIFORM(
                                                    draw_action,
                                                    m_uv_tiling,
                                                    float2,
                                                    block_in_layer->textures->tiling.x,
                                                    block_in_layer->textures->tiling.y);
                                                JE_CHECK_NEED_AND_SET_UNIFORM(
                                                    draw_action,
                                                    m_uv_offset,
                                                    float2,
                                                    block_in_layer->textures->offset.x,
                                                    block_in_layer->textures->offset.y);
                                            }
                                        }
                                    }
                                    block_in_current_layer.clear();
                                }
                            }
                        }
                    }

                    graphic::framebuffer* post_rend_target_fb =
                        current_camera.light2DPostPass->post_rend_target.value().get();
                    rend_chain = new_rend_chain_for_camera(
                        current_camera.branchPipeline, /*viewport=*/nullptr,
                        post_rend_target_fb,
                        post_rend_target_fb->width(), post_rend_target_fb->height());

                    clear_color_if_any(rend_chain, current_camera.clear);

                    const float clear_zero[4] = { 0.f, 0.f, 0.f, 0.f };
                    const float clear_infi[4] = { 0.f, 0.f, current_camera.projection->zfar, 0.f };
                    const float clear_norm[4] = { 0.f, 0.f, 1.f, 1.f };

                    // 用 zero 清空自发光通道
                    jegl_cmd_clear_color(rend_chain, 1, clear_zero);
                    // 用 infi 清空距离通道
                    jegl_cmd_clear_color(rend_chain, 2, clear_infi);
                    // 用 norm 清空法线通道
                    jegl_cmd_clear_color(rend_chain, 3, clear_norm);
                }
                else
                {
                    rend_chain = new_rend_chain_for_camera(
                        current_camera.branchPipeline, current_camera.viewport,
                        rend_aim_buffer, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);
                    clear_color_if_any(rend_chain, current_camera.clear);
                }
                // Clear depth buffer to overwrite pixels.
                jegl_cmd_clear_depth(rend_chain, 1.0);

                jegl_cmd_bind_uniform_buffer(rend_chain,
                    current_camera.projection->default_uniform_buffer->resource());

                // Walk through all entities, rend them to target buffer(include L2DCamera/R2Buf/Screen).
                draw_world_renderers(rend_chain, current_camera.frustumCulling, MAT4_VIEW, MAT4_VP);

                if (current_camera.light2DPostPass != nullptr && current_camera.shaders != nullptr)
                {
                    assert(current_camera.light2DPostPass->post_rend_target.has_value()
                        && current_camera.light2DPostPass->post_light_target.has_value());

                    // Rend Light result to target buffer.
                    graphic::framebuffer* post_light_target_fb =
                        current_camera.light2DPostPass->post_light_target.value().get();
                    jegl_command_buffer* light2d_light_effect_rend_chain = new_rend_chain_for_camera(
                        current_camera.branchPipeline, /*viewport=*/nullptr,
                        post_light_target_fb,
                        post_light_target_fb->width(), post_light_target_fb->height());

                    const float clear_color[] = { 0.f, 0.f, 0.f, 0.f };
                    jegl_cmd_clear_color(light2d_light_effect_rend_chain, 0, clear_color);
                    jegl_cmd_bind_uniform_buffer(light2d_light_effect_rend_chain,
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
                        jegl_cmd_bind_texture_2d(light2d_light_effect_rend_chain, JE_LIGHT2D_DEFER_0 + 0, diffuse_attachment);
                        jegl_cmd_bind_texture_2d(light2d_light_effect_rend_chain, JE_LIGHT2D_DEFER_0 + 1, emissive_attachment);
                        jegl_cmd_bind_texture_2d(light2d_light_effect_rend_chain, JE_LIGHT2D_DEFER_0 + 2, viewpos_attachment);
                        jegl_cmd_bind_texture_2d(light2d_light_effect_rend_chain, JE_LIGHT2D_DEFER_0 + 3, viewnorm_attachment);
                        jegl_cmd_bind_texture_2d(
                            light2d_light_effect_rend_chain, JE_LIGHT2D_DEFER_0 + 4,
                            light2d.shadowbuffer != nullptr
                            ? light2d.shadowbuffer->buffer.value()->get_attachment(0).value()->resource()
                            : m_defer_light2d_host._no_shadow->resource());

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
                        auto& drawing_shape =
                            drawing_mesh.has_value()
                            ? drawing_mesh.value()
                            : m_default_resources.default_shape_quad;
                        auto& drawing_shaders =
                            light2d.shaders->shaders.empty() == false
                            ? light2d.shaders->shaders
                            : m_default_resources.default_shaders_list;

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
                            light2d_resolution.x = (float)std::max((size_t)1, (size_t)llround(
                                WINDOWS_WIDTH * std::max(0.f, std::min(light2d.shadowbuffer->resolution_ratio, 1.0f))));
                            light2d_resolution.y = (float)std::max((size_t)1, (size_t)llround(
                                WINDOWS_HEIGHT * std::max(0.f, std::min(light2d.shadowbuffer->resolution_ratio, 1.0f))));
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
                            light2d_light_effect_rend_chain, drawing_shape, drawing_shaders, u);
                    }

                    // Rend final result color to screen.
                    jegl_command_buffer* final_target_rend_chain = new_rend_chain_for_camera(
                        current_camera.branchPipeline, current_camera.viewport,
                        rend_aim_buffer, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                    clear_color_if_any(final_target_rend_chain, current_camera.clear);
                    jegl_cmd_clear_depth(final_target_rend_chain, 1.0);
                    jegl_cmd_bind_uniform_buffer(final_target_rend_chain,
                        current_camera.projection->default_uniform_buffer->resource());

                    // 槽 0：光照合成结果；槽 4..7：G-buffer 通道
                    jegl_cmd_bind_texture_2d(final_target_rend_chain, 0,
                        current_camera.light2DPostPass->post_light_target.value()->get_attachment(0).value()->resource());
                    jegl_cmd_bind_texture_2d(final_target_rend_chain, JE_LIGHT2D_DEFER_0 + 0,
                        post_rend_target_frame_buffer->get_attachment(0).value()->resource());
                    jegl_cmd_bind_texture_2d(final_target_rend_chain, JE_LIGHT2D_DEFER_0 + 1,
                        post_rend_target_frame_buffer->get_attachment(1).value()->resource());
                    jegl_cmd_bind_texture_2d(final_target_rend_chain, JE_LIGHT2D_DEFER_0 + 2,
                        post_rend_target_frame_buffer->get_attachment(2).value()->resource());
                    jegl_cmd_bind_texture_2d(final_target_rend_chain, JE_LIGHT2D_DEFER_0 + 3,
                        post_rend_target_frame_buffer->get_attachment(3).value()->resource());

                    // 相机自带纹理（覆盖槽位由 m_pass_id 决定）
                    math::vec2 final_tiling(1.f, 1.f), final_offset(0.f, 0.f);
                    if (current_camera.textures != nullptr)
                    {
                        final_tiling = current_camera.textures->tiling;
                        final_offset = current_camera.textures->offset;
                        for (auto& texture : current_camera.textures->textures)
                            jegl_cmd_bind_texture_2d(final_target_rend_chain, texture.m_pass_id, texture.m_texture->resource());
                    }

                    auto& drawing_shaders =
                        current_camera.shaders->shaders.empty() == false
                        ? current_camera.shaders->shaders
                        : m_default_resources.default_shaders_list;

                    math::vec2 post_light_resolution(
                        (float)post_light_target_fb->width(),
                        (float)post_light_target_fb->height());
                    math::vec4 final_color = current_camera.color != nullptr
                        ? current_camera.color->color.to_linear()
                        : math::vec4(1.f, 1.f, 1.f, 1.f);

                    // 注意：合成 pass 不设置 m/mv/mvp/local_scale（这些 shader 不使用它们），
                    // 仅设置 light2d_resolution / tiling / offset / color。
                    for (auto& shader_pass : drawing_shaders)
                    {
                        auto* using_shader = &shader_pass;
                        if (!shader_pass->m_builtin)
                            using_shader = &m_default_resources.default_shader;

                        auto* vertex_resource =
                            m_defer_light2d_host._screen_vertex->resource();
                        auto* pipeline = pipeline_for(
                            (*using_shader).get(), vertex_resource);

                        render_draw_action draw_action(
                            final_target_rend_chain,
                            pipeline,
                            vertex_resource,
                            (uint32_t)vertex_resource->m_index_count,
                            1);

                        JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_light2d_resolution, float2,
                            post_light_resolution.x, post_light_resolution.y);
                        JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_uv_tiling, float2, final_tiling.x, final_tiling.y);
                        JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_uv_offset, float2, final_offset.x, final_offset.y);
                        JE_CHECK_NEED_AND_SET_UNIFORM(draw_action, m_color, float4,
                            final_color.x, final_color.y, final_color.z, final_color.w);
                    }
                } // Finish for Light2d effect.
            }
        }
    };
}
