#pragma once

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#   error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"
#include "jeecs_core_rendchain_helpers.hpp"

#include <queue>
#include <list>

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
        basic::resource<graphic::texture> default_texture;
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
            default_shader{ graphic::shader::create(
                nullptr,
                "!/builtin/builtin_default.shader", R"(
// Default shader
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex : float3,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos : float4,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        color : float4,
    };
    
public let vert =
    \v: vin = v2f{ pos = JE_MVP * vertex_pos }
        where vertex_pos = vec4!(v.vertex, 1.)
        ;
    ;
    
public let frag =
    \_: v2f = fout{ color = vec4!(t, 0., t, 1.) }
        where t = JE_TIME->y
        ;
    ;
)").value() }
, default_texture{ graphic::texture::create(2, 2, jegl_texture::format::RGBA) }
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
            rendchain_branch* branchPipeline;

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
            const Textures* textures;

            const UserInterface::Origin* ui_origin;
            const UserInterface::Rotation* ui_rotation;

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

                assert(projection.default_uniform_buffer != nullptr);

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

            assert(light_shape->resource() != nullptr);

            const auto* const raw_vertex_data = light_shape->resource();
            if (raw_vertex_data != nullptr)
            {
                size.x *= 2.0f * std::max(abs(raw_vertex_data->m_x_max), abs(raw_vertex_data->m_x_min));
                size.y *= 2.0f * std::max(abs(raw_vertex_data->m_y_max), abs(raw_vertex_data->m_y_min));
                size.z *= 2.0f * std::max(abs(raw_vertex_data->m_z_max), abs(raw_vertex_data->m_z_min));
            }

            return size;
        }

        /* ---------------- Shared render-pipeline helpers ---------------- */

        // Write the per-camera view/projection/VP/time quartet into the default uniform buffer.
        void update_default_uniform_buffer(
            const Projection& projection,
            const float (&view)[4][4],
            const float (&proj)[4][4],
            const float (&vp)[4][4],
            const math::vec4& shader_time)
        {
            assert(projection.default_uniform_buffer != nullptr);
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

        // Allocate a rend_chain for a camera; honors optional sub-viewport (normalized rect).
        jegl_rendchain* new_rend_chain_for_camera(
            rendchain_branch* branch,
            const Viewport* viewport,
            graphic::framebuffer* rend_aim_buffer,
            size_t buffer_w, size_t buffer_h)
        {
            jegl_frame_buffer* fb = rend_aim_buffer ? rend_aim_buffer->resource() : nullptr;
            if (viewport)
                return jegl_branch_new_chain(branch, fb,
                    (int32_t)(viewport->viewport.x * (float)buffer_w),
                    (int32_t)(viewport->viewport.y * (float)buffer_h),
                    (uint32_t)(viewport->viewport.z * (float)buffer_w),
                    (uint32_t)(viewport->viewport.w * (float)buffer_h));
            return jegl_branch_new_chain(branch, fb, 0, 0, 0, 0);
        }

        // Clear attachment 0 if the camera requested a clear color.
        void clear_color_if_any(jegl_rendchain* chain, const Camera::Clear* clear)
        {
            if (clear != nullptr)
            {
                const float clear_buffer_color[] = {
                    clear->color.x, clear->color.y, clear->color.z, clear->color.w };
                jegl_rchain_clear_color_buffer(chain, 0, clear_buffer_color);
            }
        }

        // Bind default texture at slot 0, then any user-supplied textures.
        // Returns the texture group plus resolved tiling/offset.
        struct bound_textures_t
        {
            jegl_rchain_texture_group* group;
            math::vec2 tiling;
            math::vec2 offset;
        };
        bound_textures_t bind_entity_textures(jegl_rendchain* chain, const Textures* textures)
        {
            bound_textures_t result;
            result.group = jegl_rchain_allocate_texture_group(chain);
            result.tiling = math::vec2(1.f, 1.f);
            result.offset = math::vec2(0.f, 0.f);

            jegl_rchain_bind_texture(
                chain, result.group, 0, m_default_resources.default_texture->resource());

            if (textures != nullptr)
            {
                result.tiling = textures->tiling;
                result.offset = textures->offset;
                for (auto& texture : textures->textures)
                    jegl_rchain_bind_texture(
                        chain, result.group, texture.m_pass_id, texture.m_texture->resource());
            }
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
            jegl_rendchain* chain,
            const basic::resource<graphic::vertex>& shape,
            const basic::vector<basic::resource<graphic::shader>>& shaders,
            jegl_rchain_texture_group* texture_group,
            const pass_uniforms_t& u)
        {
            for (auto& shader_pass : shaders)
            {
                auto* using_shader = &shader_pass;
                if (!shader_pass->m_builtin)
                    using_shader = &m_default_resources.default_shader;

                auto* rchain_draw_action = jegl_rchain_draw(
                    chain, (*using_shader)->resource(), shape->resource(), texture_group);
                auto* builtin_uniform = (*using_shader)->m_builtin;

                if (u.m != nullptr)
                    JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, u.m);
                if (u.mv != nullptr)
                    JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, u.mv);
                if (u.mvp != nullptr)
                    JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, u.mvp);

                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, local_scale, float3,
                    u.local_scale.x, u.local_scale.y, u.local_scale.z);

                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2, u.tiling.x, u.tiling.y);
                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2, u.offset.x, u.offset.y);

                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                    u.color.x, u.color.y, u.color.z, u.color.w);

                if (u.light2d_resolution != nullptr)
                    JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_resolution, float2,
                        u.light2d_resolution->x, u.light2d_resolution->y);
                if (u.light2d_decay != nullptr)
                    JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_decay, float, *u.light2d_decay);
            }
        }

        // Draw every entity in m_renderer_list using the standard world-space pipeline
        // (frustum culling -> MV/MVP -> bind_entity_textures -> draw_shader_passes).
        // Shared between UnlitGraphicPipelineSystem and DeferLight2DGraphicPipelineSystem.
        // Takes the frustum-culling handle directly so it works with any camera archetype
        // (camera_arch / l2dcamera_arch / ...) that exposes a `frustumCulling` field.
        void draw_world_renderers(
            jegl_rendchain* chain,
            const FrustumCulling* frustum_culling,
            const float (&view)[4][4],
            const float (&vp)[4][4])
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

                auto bound = bind_entity_textures(chain, rendentity.textures);

                pass_uniforms_t u;
                u.m = MAT4_MODEL;
                u.mv = MAT4_MV;
                u.mvp = MAT4_MVP;
                u.local_scale = rendentity.translation->local_scale;
                u.tiling = bound.tiling;
                u.offset = bound.offset;
                if (rendentity.color != nullptr)
                    u.color = rendentity.color->color;

                draw_shader_passes(chain, drawing_shape, drawing_shaders, bound.group, u);
            }
        }
    };

    struct UserInterfaceGraphicPipelineSystem : public BaseImpledGraphicPipeline
    {
        UserInterfaceGraphicPipelineSystem(game_world w)
            : BaseImpledGraphicPipeline(w)
        {
        }

        ~UserInterfaceGraphicPipelineSystem()
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
                m_camera_list.insert(
                    camera_arch{
                        branch, rendqueue, &projection, cameraviewport, rendbuf, nullptr, clear
                    }
                );
            }

            for (auto&& [shads, texs, shape, rendqueue, origin, rotation, color] : query<
                view typesof(Shaders&, Textures*, Shape&, Rendqueue*, Origin&, Rotation*, Color*),
                anyof typesof(Absolute, Relatively),
                except typesof(Point, Parallel, Range)
            >())
            {
                m_renderer_list.insert(
                    renderer_arch{
                        color, rendqueue, nullptr, &shape, &shads, texs, &origin, rotation });
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

                const float MAT4_UI_VIEW[4][4] = {
                    {2.0f / (float)RENDAIMBUFFER_WIDTH, 0.0f, 0.0f, 0.0f},
                    {0.0f, 2.0f / (float)RENDAIMBUFFER_HEIGHT, 0.0f, 0.0f},
                    {0.0f, 0.0f, 1.0f, 0.0f},
                    {0.0f, 0.0f, 0.0f, 1.0f},
                };
                // For UI, vp == view (projection is identity), so view-projection == view.
                update_default_uniform_buffer(
                    *current_camera.projection, MAT4_UI_VIEW, MAT4_UI_UNIT, MAT4_UI_VIEW, shader_time);

                jegl_rendchain* rend_chain = new_rend_chain_for_camera(
                    current_camera.branchPipeline, current_camera.viewport,
                    rend_aim_buffer, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                clear_color_if_any(rend_chain, current_camera.clear);
                jegl_rchain_clear_depth_buffer(rend_chain, 1.0f);
                jegl_rchain_bind_uniform_buffer(rend_chain,
                    current_camera.projection->default_uniform_buffer->resource());

                for (auto& rendentity : m_renderer_list)
                {
                    assert(rendentity.ui_origin != nullptr && rendentity.shaders != nullptr && rendentity.shape != nullptr);

                    auto& drawing_shape =
                        rendentity.shape->vertex.has_value()
                        ? rendentity.shape->vertex.value()
                        : m_default_resources.default_shape_quad;
                    auto& drawing_shaders =
                        rendentity.shaders->shaders.empty() == false
                        ? rendentity.shaders->shaders
                        : m_default_resources.default_shaders_list;

                    math::vec2 uioffset, uisize, uicenteroffset;
                    rendentity.ui_origin->get_layout(
                        (float)RENDAIMBUFFER_WIDTH,
                        (float)RENDAIMBUFFER_HEIGHT,
                        &uioffset, &uisize, &uicenteroffset);

                    uioffset.x -= (float)RENDAIMBUFFER_WIDTH / 2.0f;
                    uioffset.y -= (float)RENDAIMBUFFER_HEIGHT / 2.0f;

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

                    if (rendentity.ui_rotation != nullptr)
                    {
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
                        math::quat q(0.0f, 0.0f, rendentity.ui_rotation->angle);
                        q.create_matrix(MAT4_UI_ROTATION);

                        math::mat4xmat4(MAT4_UI_MV /* tmp */, MAT4_UI_ROTATION, MAT4_UI_CENTER_OFFSET);
                        math::mat4xmat4(MAT4_UI_ROTATION, MAT4_UI_INV_CENTER_OFFSET, MAT4_UI_MV /* tmp */);
                    }
                    math::mat4xmat4(MAT4_UI_MV /* tmp */, MAT4_UI_OFFSET, MAT4_UI_ROTATION);
                    math::mat4xmat4(MAT4_UI_MODEL, MAT4_UI_MV /* tmp */, MAT4_UI_SIZE);
                    math::mat4xmat4(MAT4_UI_MV, MAT4_UI_VIEW, MAT4_UI_MODEL);

                    auto bound = bind_entity_textures(rend_chain, rendentity.textures);

                    pass_uniforms_t u;
                    u.m = MAT4_UI_MODEL;
                    // UI uses MV for both mvp and mv slots.
                    u.mv = MAT4_UI_MV;
                    u.mvp = MAT4_UI_MV;
                    u.tiling = bound.tiling;
                    u.offset = bound.offset;
                    if (rendentity.color != nullptr)
                        u.color = rendentity.color->color;

                    draw_shader_passes(rend_chain, drawing_shape, drawing_shaders, bound.group, u);
                }
            }
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
                m_camera_list.insert(
                    camera_arch{
                        branch, rendqueue, &projection, cameraviewport, rendbuf, frustumCulling, clear
                    }
                );
            }

            for (auto&& [trans, shads, texs, shape, rendqueue, color] : query<
                view typesof(
                    Translation&,
                    Shaders&,
                    Textures*,
                    Shape&,
                    Rendqueue*,
                    Color*
                ),
                except typesof(
                    Point,
                    Parallel,
                    Origin
                )
            >())
            {
                // RendOb will be input to a chain and used for swap
                m_renderer_list.insert(
                    renderer_arch{
                        color, rendqueue, &trans, &shape, &shads, texs
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

                jegl_rendchain* rend_chain = new_rend_chain_for_camera(
                    current_camera.branchPipeline, current_camera.viewport,
                    rend_aim_buffer, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                clear_color_if_any(rend_chain, current_camera.clear);
                jegl_rchain_clear_depth_buffer(rend_chain, 1.0);
                jegl_rchain_bind_uniform_buffer(rend_chain,
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
            jeecs::basic::resource<jeecs::graphic::texture> _no_shadow;
            jeecs::basic::resource<jeecs::graphic::vertex> _screen_vertex;

            jeecs::basic::resource<jeecs::graphic::vertex> _sprite_shadow_vertex;

            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_point_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_parallel_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_point_reverse_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_parallel_reverse_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_shape_point_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_shape_parallel_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sprite_point_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sprite_parallel_pass;

            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sub_pass;

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
            inline static const float _sprite_shadow_vertex_data[] = {
                -0.5f,
                -0.5f,
                0.0f,
                0.0f,
                1.0f,
                1.0f,
                -0.5f,
                -0.5f,
                0.0f,
                0.0f,
                0.0f,
                0.0f,
                0.5f,
                -0.5f,
                0.0f,
                1.0f,
                1.0f,
                1.0f,
                0.5f,
                -0.5f,
                0.0f,
                1.0f,
                0.0f,
                0.0f,
            };
            DeferLight2DResource()
                : _no_shadow{
                    jeecs::graphic::texture::create(1, 1, jegl_texture::format::RGBA)
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
                , _sprite_shadow_vertex{
                    jeecs::graphic::vertex::create(
                        jegl_vertex::TRIANGLESTRIP,
                        _sprite_shadow_vertex_data,
                        sizeof(_sprite_shadow_vertex_data),
                        {0, 1, 2, 3},
                        {
                            {jegl_vertex::data_type::FLOAT32, 3},
                            {jegl_vertex::data_type::FLOAT32, 2},
                            {jegl_vertex::data_type::FLOAT32, 1},
                        })
                        .value()
                }
                , _defer_light2d_shadow_point_pass{
                    jeecs::graphic::shader::create(
                        nullptr,
                        "!/builtin/defer_light2d_shadow_point.shader",
                        R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (MAX, ONE, ONE);
CULL    (BACK);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        factor: float,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos: float4,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        shadow_factor: float4,
    };
    
public func vert(v: vin)
{
    // ATTENTION: We will using JE_COLOR: float4 to pass lwpos.
    let light_vpos = JE_V * JE_COLOR;
    let vpos = JE_MV * vec4!(v.vertex, 1.);
    
    let shadow_vdir = vec3!(normalize(vpos->xy - light_vpos->xy), 0.) * 2000. * v.factor;
    
    return v2f{pos = JE_P * vec4!(vpos->xyz + shadow_vdir, 1.)};
}

public func frag(_: v2f)
{
    // NOTE: JE_LOCAL_SCALE->x is shadow factor here.
    let shadow_factor = JE_LOCAL_SCALE->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, shadow_factor),
    };
}
)").value()
                }
                , _defer_light2d_shadow_parallel_pass{
                    jeecs::graphic::shader::create(
                        nullptr,
                        "!/builtin/defer_light2d_shadow_parallel.shader",
                        R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (MAX, ONE, ONE);
CULL    (BACK);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        factor: float,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos: float4,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        shadow_factor: float4,
    };
    
public func vert(v: vin)
{
    // ATTENTION: We will using JE_COLOR: float4 to pass lwpos.
    let light_vdir = (JE_V * JE_COLOR)->xyz - movement(JE_V);
    let vpos = JE_MV * vec4!(v.vertex, 1.);
    
    let shadow_vdir = vec3!(normalize(light_vdir->xy), 0.) * 2000. * v.factor;
    
    return v2f{pos = JE_P * vec4!(vpos->xyz + shadow_vdir, 1.)};
}

public func frag(_: v2f)
{
    // NOTE: JE_LOCAL_SCALE->x is shadow factor here.
    let shadow_factor = JE_LOCAL_SCALE->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, shadow_factor),
    };
}
)").value()
                }
                , _defer_light2d_shadow_point_reverse_pass{
                jeecs::graphic::shader::create(
                    nullptr,
                    "!/builtin/defer_light2d_shadow_reverse_point.shader",
                    R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (MAX, ONE, ONE);
CULL    (FRONT);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        factor: float,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos: float4,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        shadow_factor: float4,
    };
    
public func vert(v: vin)
{
    // ATTENTION: We will using JE_COLOR: float4 to pass lwpos.
    let light_vpos = JE_V * JE_COLOR;
    let vpos = JE_MV * vec4!(v.vertex, 1.);
    
    let shadow_vdir = vec3!(normalize(vpos->xy - light_vpos->xy), 0.) * 2000. * v.factor;
    
    return v2f{pos = JE_P * vec4!(vpos->xyz + shadow_vdir, 1.)};
}

public func frag(_: v2f)
{
    // NOTE: JE_LOCAL_SCALE->x is shadow factor here.
    let shadow_factor = JE_LOCAL_SCALE->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, shadow_factor),
    };
}
)").value()
                }
                , _defer_light2d_shadow_parallel_reverse_pass{
                    jeecs::graphic::shader::create(
                        nullptr,
                        "!/builtin/defer_light2d_shadow_reverse_parallel.shader",
                        R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (MAX, ONE, ONE);
CULL    (FRONT);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        factor: float,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct{
        pos: float4,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct{
        shadow_factor: float4,
    };
    
public func vert(v: vin)
{
    // ATTENTION: We will using JE_COLOR: float4 to pass lwpos.
    let light_vdir = (JE_V * JE_COLOR)->xyz - movement(JE_V);
    let vpos = JE_MV * vec4!(v.vertex, 1.);
    
    let shadow_vdir = vec3!(normalize(light_vdir->xy), 0.) * 2000. * v.factor;
    
    return v2f{pos = JE_P * vec4!(vpos->xyz + shadow_vdir, 1.)};
}

public func frag(_: v2f)
{
    // NOTE: JE_LOCAL_SCALE->x is shadow factor here.
    let shadow_factor = JE_LOCAL_SCALE->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, shadow_factor),
    };
}
)").value()
                }
                , _defer_light2d_shadow_shape_point_pass{
                    jeecs::graphic::shader::create(
                        nullptr,
                        "!/builtin/defer_light2d_shadow_point_shape.shader",
                        R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (MAX, ONE, ONE);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        uv: float2,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos: float4,
        uv: float2,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        shadow_factor: float4,
    };
    
public func vert(v: vin)
{
    let light2d_vpos = JE_V * vec4!(JE_COLOR->xyz, 1.);
    let shadow_scale_factor = JE_COLOR->w;
    
    let vpos = JE_MV * vec4!(v.vertex, 1.);
    let vpos_light_diff = vpos->xyz / vpos->w - light2d_vpos->xyz / light2d_vpos->w;
    let vpos_light_diff_z_abs =  abs(vpos_light_diff->z);
    let shadow_offset_xy_z_abs = vec3!(
        vpos_light_diff->xy / (1. + vpos_light_diff_z_abs) * shadow_scale_factor,
        vpos_light_diff_z_abs);
        
    let shadow_tiling_scale = JE_LOCAL_SCALE->yz;
    let shadow_tiling_scale_apply_offset =
        (vec2!(0.5, 0.5) - v.uv) * 2. * (vec2!(1., 1.) - shadow_tiling_scale);
        
    return v2f{
        pos = JE_P * vec4!((vpos->xyz / vpos->w) + shadow_offset_xy_z_abs, 1.),
        uv = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET) + shadow_tiling_scale_apply_offset,
    };
}

let linear_clamp = Sampler2D::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main = texture2d::uniform(0, linear_clamp);
    
public func frag(vf: v2f)
{
    let final_shadow = alphatest(
        vec4!(
            JE_LOCAL_SCALE,     // NOTE: JE_LOCAL_SCALE->x is shadow factor here.
            tex2d(Main, vf.uv)->w));
            
    let shadow_factor = final_shadow->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, shadow_factor),
    };
}
)").value()
                }
                , _defer_light2d_shadow_shape_parallel_pass{
                    jeecs::graphic::shader::create(
                        nullptr,
                        "!/builtin/defer_light2d_shadow_parallel_shape.shader",
                        R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (MAX, ONE, ONE);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        uv: float2,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct{
        pos: float4,
        uv: float2,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct{
        shadow_factor: float4,
    };
    
public func vert(v: vin)
{
    let light2d_vdir = (JE_V * vec4!(JE_COLOR->xyz, 1.))->xyz - movement(JE_V);
    let shadow_scale_factor = JE_COLOR->w;
    
    let vpos = JE_MV * vec4!(v.vertex, 1.);
    let shadow_vpos = normalize(light2d_vdir) * shadow_scale_factor;
    
    let shadow_tiling_scale = JE_LOCAL_SCALE->yz;
    let shadow_tiling_scale_apply_offset =
        (vec2!(0.5, 0.5) - v.uv) * 2. * (vec2!(1., 1.) - shadow_tiling_scale);
        
    return v2f{
        pos = JE_P * vec4!((vpos->xyz / vpos->w) + shadow_vpos, 1.),
        uv = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET) + shadow_tiling_scale_apply_offset,
    };
}

let linear_clamp = Sampler2D::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main = texture2d::uniform(0, linear_clamp);
    
public func frag(vf: v2f)
{
    let final_shadow = alphatest(
        vec4!(
            JE_LOCAL_SCALE,     // NOTE: JE_LOCAL_SCALE->x is shadow factor here.
            tex2d(Main, vf.uv)->w));
            
    let shadow_factor = final_shadow->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, shadow_factor),
    };
}
)").value()
                }
                , _defer_light2d_shadow_sprite_point_pass{
                    jeecs::graphic::shader::create(
                        nullptr,
                        "!/builtin/defer_light2d_shadow_point_sprite.shader",
                        R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (MAX, ONE, ONE);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        uv: float2,
        factor: float,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct{
        pos: float4,
        uv: float2,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct{
        shadow_factor: float4,
    };
    
public func vert(v: vin)
{
    let light2d_vpos = JE_V * vec4!(JE_COLOR->xyz, 1.);
    let shadow_scale_factor = JE_COLOR->w;
    
    let vpos = JE_MV * vec4!(v.vertex, 1.);
    let centerpos = JE_MV * vec4!(0., 0., 0., 1.);
    let shadow_vpos = normalize(
        (centerpos->xyz / centerpos->w) - (light2d_vpos->xyz / light2d_vpos->w)
        ) * shadow_scale_factor;
        
    return v2f{
        pos = JE_P * vec4!((vpos->xyz / vpos->w) + shadow_vpos * v.factor, 1.),
        uv = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET),
    };
}

let linear_clamp = Sampler2D::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main = texture2d::uniform(0, linear_clamp);
    
public func frag(vf: v2f)
{

    let final_shadow = alphatest(
        vec4!(
            JE_LOCAL_SCALE,     // NOTE: JE_LOCAL_SCALE->x is shadow factor here.
            tex2d(Main, vf.uv)->w));
            
    let shadow_factor = final_shadow->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, shadow_factor),
    };
}
)").value()
                }
                , _defer_light2d_shadow_sprite_parallel_pass{
                jeecs::graphic::shader::create(
                    nullptr,
                    "!/builtin/defer_light2d_shadow_parallel_sprite.shader"
                    , R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (MAX, ONE, ONE);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        uv: float2,
        factor: float,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos: float4,
        uv: float2,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        shadow_factor: float4,
    };
    
public func vert(v: vin)
{
    let light2d_vdir = (JE_V * vec4!(JE_COLOR->xyz, 1.))->xyz - movement(JE_V);
    let shadow_scale_factor = JE_COLOR->w;
    
    let vpos = JE_MV * vec4!(v.vertex, 1.);
    let shadow_vpos = normalize(light2d_vdir) * shadow_scale_factor;
    
    return v2f{
        pos = JE_P * vec4!((vpos->xyz / vpos->w) + shadow_vpos * v.factor, 1.),
        uv = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET),
    };
}

let linear_clamp = Sampler2D::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main = texture2d::uniform(0, linear_clamp);
    
public func frag(vf: v2f)
{
    let final_shadow = alphatest(
        vec4!(
            JE_LOCAL_SCALE,     // NOTE: JE_LOCAL_SCALE->x is shadow factor here.
            tex2d(Main, vf.uv)->w));
            
    let shadow_factor = final_shadow->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, shadow_factor),
    };
}
)").value()
                }
                , _defer_light2d_shadow_sub_pass{
                    jeecs::graphic::shader::create(
                        nullptr,
                        "!/builtin/defer_light2d_shadow_sub.shader",
                        R"(
import woo::std;

import je::shader;
import pkg::woshader;

using woshader;
using je::shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

WOSHADER_VERTEX_IN!
    using vin = struct {
        vertex: float3,
        uv: float2,
    };
    
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct {
        pos: float4,
        uv: float2,
    };
    
WOSHADER_FRAGMENT_OUT!
    using fout = struct {
        shadow_factor: float4,
    };
    
public func vert(v: vin)
{
    return v2f{
        pos = JE_MVP * vec4!(v.vertex, 1.),
        uv = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET),
    };
}

let nearest_clamp = Sampler2D::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main = texture2d::uniform(0, nearest_clamp);
    
public func frag(vf: v2f)
{
    let final_shadow = alphatest(vec4!(JE_COLOR->xyz, tex2d(Main, vf.uv)->w));
    
    let shadow_factor = final_shadow->x;
    return fout{
        shadow_factor = vec4!(
            shadow_factor, shadow_factor, shadow_factor, 1.),
    };
}
)").value()
                }
            {
                // Donothing...
            }
        };

        DeferLight2DResource m_defer_light2d_host;

        struct l2dcamera_arch
        {
            rendchain_branch* branchPipeline;

            const Rendqueue* rendqueue;
            const Translation* translation;
            const Projection* projection;
            const Viewport* viewport;
            const RendToFramebuffer* rendToFramebuffer;
            const Light2D::CameraPostPass* light2DPostPass;
            const Shaders* shaders;
            const Textures* textures;
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
            const Light2D::Gain* gain;
            const Light2D::ShadowBuffer* shadowbuffer;
            const Renderer::Color* color;
            const Shape* shape;
            const Shaders* shaders;
            const Textures* textures;
        };

        struct block2d_arch
        {
            const Translation* translation;

            const Light2D::BlockShadow* blockshadow;
            const Light2D::ShapeShadow* shapeshadow;
            const Light2D::SpriteShadow* spriteshadow;
            const Light2D::SelfShadow* selfshadow;

            const Textures* textures;
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
                    Textures*,
                    FrustumCulling*,
                    Clear*,
                    Color*
                )
            >())
            {
                auto* branch = this->allocate_branch(rendqueue == nullptr ? 0 : rendqueue->rend_queue);
                m_2dcamera_list.insert(
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
                    }
                );

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
                                    jegl_texture::format::RGBA,
                                    // 自发光颜色，用于法线反射或者发光物体的颜色参数，最终混合shader会将此参数用于光照计算
                                    jegl_texture::format(jegl_texture::format::RGBA | jegl_texture::format::FLOAT16),
                                    // 视空间坐标(RGB) Alpha通道暂时留空
                                    jegl_texture::format(jegl_texture::format::RGBA | jegl_texture::format::FLOAT16),
                                    // 法线空间颜色
                                    jegl_texture::format(jegl_texture::format::RGBA | jegl_texture::format::FLOAT16),
                                },
                                true).value();
                        light2dpostpass->post_light_target
                            = jeecs::graphic::framebuffer::create(
                                LIGHT_BUFFER_WIDTH,
                                LIGHT_BUFFER_HEIGHT,
                                {
                                    // 光渲染结果
                                    (jegl_texture::format)(jegl_texture::format::RGBA | jegl_texture::format::FLOAT16),
                                },
                                false).value();
                    }
                }
            }

            for (auto&& [trans, shads, texs, shape, rendqueue, color] : query<
                view typesof(
                    Translation&,
                    Shaders&,
                    Textures*,
                    Shape&,
                    Rendqueue*,
                    Color*
                ),
                except typesof(
                    Point,
                    Parallel,
                    Range,
                    Origin
                )
            >())
            {
                // RendOb will be input to a chain and used for swap
                m_renderer_list.insert(
                    renderer_arch{
                        color, rendqueue, &trans, &shape, &shads, texs
                    });
            }

            for (auto&& [
                trans,
                topdown,
                point,
                parallel,
                range,
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
                    Gain*,
                    ShadowBuffer*,
                    Color*,
                    Shape&,
                    Shaders&,
                    Textures*
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
                        &trans, topdown, point, parallel, range, gain, shadowbuffer,
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
                                jegl_texture::format::RGBA,
                                // Only store shadow value to R-pass
                            },
                            false).value();
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

            for (auto&& [trans, blockshadow, shapeshadow, spriteshadow, selfshadow, texture, shape] : query<
                view typesof(
                    Translation&,
                    BlockShadow*,
                    ShapeShadow*,
                    SpriteShadow*,
                    SelfShadow*,
                    Textures*,
                    Shape&
                ),
                anyof typesof(
                    BlockShadow,
                    ShapeShadow,
                    SpriteShadow,
                    SelfShadow
                )
            >())
            {
                if (blockshadow != nullptr)
                {
                    if (!blockshadow->mesh.m_block_mesh.has_value())
                    {
                        std::vector<float> _vertex_buffer;
                        std::vector<uint32_t> _index_buffer;
                        if (!blockshadow->mesh.m_block_points.empty())
                        {
                            for (auto& point : blockshadow->mesh.m_block_points)
                            {
                                _vertex_buffer.insert(_vertex_buffer.end(),
                                    {
                                        point.x, point.y, 0.f, 0.f,
                                        point.x, point.y, 0.f, 1.f,
                                    });

                                uint32_t _index_offset = _index_buffer.size();
                                _index_buffer.insert(_index_buffer.end(),
                                    {
                                        _index_offset,
                                        _index_offset + 1,
                                    });
                            }
                            blockshadow->mesh.m_block_mesh = jeecs::graphic::vertex::create(
                                jegl_vertex::type::TRIANGLESTRIP,
                                _vertex_buffer.data(), _vertex_buffer.size() * sizeof(float),
                                _index_buffer,
                                {
                                    {jegl_vertex::data_type::FLOAT32, 3},
                                    {jegl_vertex::data_type::FLOAT32, 1},
                                });
                        }
                        else
                            blockshadow->mesh.m_block_mesh.reset();
                    }
                }
                m_2dblock_z_list.push_back(
                    block2d_arch{
                        &trans,
                        blockshadow,
                        shapeshadow,
                        spriteshadow,
                        selfshadow,
                        texture,
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
            std::sort(m_2dblock_y_list.begin(), m_2dblock_y_list.end(),
                [](const block2d_arch& a, const block2d_arch& b)
                {
                    return a.translation->world_position.y > b.translation->world_position.y;
                });

            this->branch_allocate_end();
            DrawFrame();
        }

        /* ------- DeferLight2D-specific helpers (use the base helpers too) ------- */

        // Pick the appropriate 5-tuple of shadow shaders based on light type (point/parallel).
        struct defer_light2d_shadow_passes_t
        {
            const basic::resource<graphic::shader>& normal;
            const basic::resource<graphic::shader>& reverse_normal;
            const basic::resource<graphic::shader>& shape;
            const basic::resource<graphic::shader>& sprite;
            const basic::resource<graphic::shader>& sub;
        };
        defer_light2d_shadow_passes_t select_shadow_passes(bool is_parallel) const
        {
            return is_parallel
                ? defer_light2d_shadow_passes_t{
                    m_defer_light2d_host._defer_light2d_shadow_parallel_pass,
                    m_defer_light2d_host._defer_light2d_shadow_parallel_reverse_pass,
                    m_defer_light2d_host._defer_light2d_shadow_shape_parallel_pass,
                    m_defer_light2d_host._defer_light2d_shadow_sprite_parallel_pass,
                    m_defer_light2d_host._defer_light2d_shadow_sub_pass,
                }
                : defer_light2d_shadow_passes_t{
                    m_defer_light2d_host._defer_light2d_shadow_point_pass,
                    m_defer_light2d_host._defer_light2d_shadow_point_reverse_pass,
                    m_defer_light2d_host._defer_light2d_shadow_shape_point_pass,
                    m_defer_light2d_host._defer_light2d_shadow_sprite_point_pass,
                    m_defer_light2d_host._defer_light2d_shadow_sub_pass,
                };
        }

        // Allocate a texture group and bind slot 0 to the block's main texture,
        // or to the default texture if the block has Textures but no main slot configured.
        // Returns nullptr-like behavior (group is allocated but slot 0 is left unset) when
        // the block has no Textures component at all — matching the original semantics.
        jegl_rchain_texture_group* alloc_shadow_texture_group(
            jegl_rendchain* chain, const Textures* textures)
        {
            auto* group = jegl_rchain_allocate_texture_group(chain);
            if (textures != nullptr)
            {
                auto main_texture = textures->get_texture(0);
                jegl_rchain_bind_texture(
                    chain, group, 0,
                    main_texture.has_value()
                        ? main_texture.value()->resource()
                        : m_default_resources.default_texture->resource());
            }
            return group;
        }

        // Unified draw call for the 3 Light2D shadow occluder types (block / shape / sprite).
        // They all compute MV/MVP from the occluder's object2world and pack shadow params
        // into local_scale + color. Block uses (0,-1,1).unit() as parallel light dir ref;
        // shape/sprite use (0,-1,0).
        void draw_shadow_occluder(
            jegl_rendchain* shadow_chain,
            const basic::resource<graphic::shader>& shadow_shader,
            const basic::resource<graphic::vertex>& shape_vertex,
            jegl_rchain_texture_group* texture_group,
            const Translation* occluder_translation,
            const light2d_arch& light,
            const float (&view)[4][4],
            const float (&vp)[4][4],
            math::vec3 local_scale,
            float color_w,
            const math::vec3& parallel_dir_ref,
            const Textures* textures)
        {
            auto* rchain_draw_action = jegl_rchain_draw(
                shadow_chain, shadow_shader->resource(), shape_vertex->resource(), texture_group);
            auto* builtin_uniform = shadow_shader->m_builtin;

            const float(&MAT4_MODEL)[4][4] = occluder_translation->object2world;
            float MAT4_MVP[4][4], MAT4_MV[4][4];
            math::mat4xmat4(MAT4_MVP, vp, MAT4_MODEL);
            math::mat4xmat4(MAT4_MV, view, MAT4_MODEL);

            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_MODEL);
            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_MV);
            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_MVP);
            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, local_scale, float3,
                local_scale.x, local_scale.y, local_scale.z);

            if (textures != nullptr)
            {
                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2,
                    textures->tiling.x, textures->tiling.y);
                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2,
                    textures->offset.x, textures->offset.y);
            }

            // 通过 je_color 变量传递光源位置（点光）或方向（平行光）
            if (light.parallel != nullptr)
            {
                math::vec3 rotated_light_dir = light.translation->world_rotation * parallel_dir_ref;
                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                    rotated_light_dir.x, rotated_light_dir.y, rotated_light_dir.z, color_w);
            }
            else
            {
                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                    light.translation->world_position.x,
                    light.translation->world_position.y,
                    light.translation->world_position.z,
                    color_w);
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

                jegl_rendchain* rend_chain = nullptr;

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

                            auto& light2d_shadow_aim_buffer = lightarch.shadowbuffer->buffer.value();
                            jegl_rendchain* light2d_shadow_rend_chain = new_rend_chain_for_camera(
                                current_camera.branchPipeline,
                                /*viewport=*/nullptr,
                                light2d_shadow_aim_buffer.get(),
                                light2d_shadow_aim_buffer->width(),
                                light2d_shadow_aim_buffer->height());

                            const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
                            jegl_rchain_clear_color_buffer(light2d_shadow_rend_chain, 0, clear_color);

                            jegl_rchain_bind_uniform_buffer(light2d_shadow_rend_chain,
                                current_camera.projection->default_uniform_buffer->resource());

                            const auto shadow_passes = select_shadow_passes(lightarch.parallel != nullptr);

                            std::list<block2d_arch*> block_in_current_layer;

                            auto block2d_iter = lightarch.topdown == nullptr ? m_2dblock_z_list.begin() : m_2dblock_y_list.begin();
                            auto block2d_end = lightarch.topdown == nullptr ? m_2dblock_z_list.end() : m_2dblock_y_list.end();

                            for (; block2d_iter != block2d_end; ++block2d_iter)
                            {
                                auto& blockarch = *block2d_iter;

                                int64_t current_layer = lightarch.topdown == nullptr
                                    ? (int64_t)(blockarch.translation->world_position.z * 100.f)
                                    : (int64_t)(blockarch.translation->world_position.y * 100.f);

                                const float block_range = 0.5f *
                                    get_entity_size(*blockarch.translation, blockarch.shape->vertex).length();

                                const auto l2b_distance = (math::vec2(blockarch.translation->world_position) -
                                    math::vec2(lightarch.translation->world_position))
                                    .length();

                                if (lightarch.parallel != nullptr || l2b_distance <= block_range + light_range)
                                {
                                    block_in_current_layer.push_back(&blockarch);

                                    bool light_is_above_block = lightarch.topdown == nullptr
                                        ? blockarch.translation->world_position.z >= lightarch.translation->world_position.z
                                        : blockarch.translation->world_position.y >= lightarch.translation->world_position.y;

                                    if (blockarch.blockshadow != nullptr
                                        && blockarch.blockshadow->factor > 0.f
                                        && (!light_is_above_block || !blockarch.blockshadow->auto_disable)
                                        && blockarch.blockshadow->mesh.m_block_mesh.has_value())
                                    {
                                        // BlockShadow 用 (0,-1,1).unit() 作为平行光方向参考，
                                        // 通过 local_scale.x 传递阴影权重，color.w 固定为 1。
                                        const auto& pass = blockarch.blockshadow->reverse
                                            ? shadow_passes.reverse_normal
                                            : shadow_passes.normal;

                                        draw_shadow_occluder(
                                            light2d_shadow_rend_chain,
                                            pass,
                                            blockarch.blockshadow->mesh.m_block_mesh.value(),
                                            /*texture_group=*/nullptr,
                                            blockarch.translation,
                                            lightarch,
                                            MAT4_VIEW, MAT4_VP,
                                            math::vec3(blockarch.blockshadow->factor, 0.f, 0.f),
                                            /*color_w=*/1.f,
                                            math::vec3(0.f, -1.f, 1.f).unit(),
                                            blockarch.textures);
                                    }
                                    if (blockarch.shapeshadow != nullptr
                                        && blockarch.shapeshadow->factor > 0.f
                                        && (light_is_above_block || !blockarch.shapeshadow->auto_disable))
                                    {
                                        assert(blockarch.shape != nullptr);
                                        const auto& using_shape = blockarch.shape->vertex.has_value()
                                            ? blockarch.shape->vertex.value()
                                            : m_default_resources.default_shape_quad;
                                        auto* texture_group = alloc_shadow_texture_group(
                                            light2d_shadow_rend_chain, blockarch.textures);

                                        // ShapeShadow 通过 local_scale.x 传权重，.yz 传 tiling_scale，
                                        // color.w 传 distance，平行光方向参考 (0,-1,0)。
                                        draw_shadow_occluder(
                                            light2d_shadow_rend_chain,
                                            shadow_passes.shape,
                                            using_shape,
                                            texture_group,
                                            blockarch.translation,
                                            lightarch,
                                            MAT4_VIEW, MAT4_VP,
                                            math::vec3(
                                                blockarch.shapeshadow->factor,
                                                blockarch.shapeshadow->tiling_scale.x,
                                                blockarch.shapeshadow->tiling_scale.y),
                                            blockarch.shapeshadow->distance,
                                            math::vec3(0.f, -1.f, 0.f),
                                            blockarch.textures);
                                    }
                                    if (blockarch.spriteshadow != nullptr && blockarch.spriteshadow->factor > 0.f)
                                    {
                                        auto* texture_group = alloc_shadow_texture_group(
                                            light2d_shadow_rend_chain, blockarch.textures);

                                        // SpriteShadow 通过 local_scale.x 传权重，.yz 预留，
                                        // color.w 传 distance，平行光方向参考 (0,-1,0)。
                                        draw_shadow_occluder(
                                            light2d_shadow_rend_chain,
                                            shadow_passes.sprite,
                                            m_defer_light2d_host._sprite_shadow_vertex,
                                            texture_group,
                                            blockarch.translation,
                                            lightarch,
                                            MAT4_VIEW, MAT4_VP,
                                            math::vec3(blockarch.spriteshadow->factor, 0.f, 0.f),
                                            blockarch.spriteshadow->distance,
                                            math::vec3(0.f, -1.f, 0.f),
                                            blockarch.textures);
                                    }
                                }

                                // 如果下一个阴影将会在不同层级，或者当前阴影是最后一个阴影，则更新阴影覆盖
                                auto next_block2d_iter = block2d_iter + 1;
                                if (next_block2d_iter == block2d_end || current_layer != (int64_t)((lightarch.topdown == nullptr
                                    ? next_block2d_iter->translation->world_position.z
                                    : next_block2d_iter->translation->world_position.y) *
                                    100.f))
                                {
                                    for (auto* block_in_layer : block_in_current_layer)
                                    {
                                        // 如果物体被指定为不需要cover，那么就不绘制
                                        if (block_in_layer->selfshadow != nullptr)
                                        {
                                            auto* texture_group = alloc_shadow_texture_group(
                                                light2d_shadow_rend_chain, block_in_layer->textures);

                                            assert(block_in_layer->shape != nullptr);
                                            const auto& using_shape =
                                                block_in_layer->shape->vertex.has_value()
                                                ? block_in_layer->shape->vertex.value()
                                                : m_default_resources.default_shape_quad;

                                            auto* rchain_draw_action = jegl_rchain_draw(
                                                light2d_shadow_rend_chain,
                                                shadow_passes.sub->resource(),
                                                using_shape->resource(),
                                                texture_group);
                                            auto* builtin_uniform = shadow_passes.sub->m_builtin;

                                            const float(&MAT4_MODEL)[4][4] = block_in_layer->translation->object2world;
                                            float MAT4_MVP[4][4], MAT4_MV[4][4];
                                            math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                                            math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_MODEL);
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_MV);
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_MVP);

                                            // local_scale.x .y .z 通道预留
                                            /*NEED_AND_SET_UNIFORM(local_scale, float3,
                                                0.f,
                                                0.f,
                                                0.f);*/

                                            if (block_in_layer->selfshadow->auto_disable &&
                                                (lightarch.topdown == nullptr
                                                    ? block_in_layer->translation->world_position.z >= lightarch.translation->world_position.z
                                                    : block_in_layer->translation->world_position.y >= lightarch.translation->world_position.y))
                                                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4, 0.f, 0.f, 0.f, 0.f);
                                            else
                                                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                                    block_in_layer->selfshadow->factor,
                                                    block_in_layer->selfshadow->factor,
                                                    block_in_layer->selfshadow->factor,
                                                    block_in_layer->selfshadow->factor);

                                            if (block_in_layer->textures != nullptr)
                                            {
                                                JE_CHECK_NEED_AND_SET_UNIFORM(
                                                    rchain_draw_action,
                                                    builtin_uniform,
                                                    tiling,
                                                    float2,
                                                    block_in_layer->textures->tiling.x,
                                                    block_in_layer->textures->tiling.y);
                                                JE_CHECK_NEED_AND_SET_UNIFORM(
                                                    rchain_draw_action,
                                                    builtin_uniform,
                                                    offset,
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
                    jegl_rchain_clear_color_buffer(rend_chain, 1, clear_zero);
                    // 用 infi 清空距离通道
                    jegl_rchain_clear_color_buffer(rend_chain, 2, clear_infi);
                    // 用 norm 清空法线通道
                    jegl_rchain_clear_color_buffer(rend_chain, 3, clear_norm);
                }
                else
                {
                    rend_chain = new_rend_chain_for_camera(
                        current_camera.branchPipeline, current_camera.viewport,
                        rend_aim_buffer, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);
                    clear_color_if_any(rend_chain, current_camera.clear);
                }
                // Clear depth buffer to overwrite pixels.
                jegl_rchain_clear_depth_buffer(rend_chain, 1.0);

                jegl_rchain_bind_uniform_buffer(rend_chain,
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
                    jegl_rendchain* light2d_light_effect_rend_chain = new_rend_chain_for_camera(
                        current_camera.branchPipeline, /*viewport=*/nullptr,
                        post_light_target_fb,
                        post_light_target_fb->width(), post_light_target_fb->height());

                    const float clear_color[] = { 0.f, 0.f, 0.f, 0.f };
                    jegl_rchain_clear_color_buffer(light2d_light_effect_rend_chain, 0, clear_color);
                    jegl_rchain_bind_uniform_buffer(light2d_light_effect_rend_chain,
                        current_camera.projection->default_uniform_buffer->resource());

                    auto* post_rend_target_frame_buffer = current_camera.light2DPostPass->post_rend_target.value().get();
                    auto* diffuse_attachment = post_rend_target_frame_buffer->get_attachment(0).value()->resource();
                    auto* emissive_attachment = post_rend_target_frame_buffer->get_attachment(1).value()->resource();
                    auto* viewpos_attachment = post_rend_target_frame_buffer->get_attachment(2).value()->resource();
                    auto* viewnorm_attachment = post_rend_target_frame_buffer->get_attachment(3).value()->resource();

                    for (auto* light2d_p : _2dlight_after_culling)
                    {
                        auto& light2d = *light2d_p;

                        assert(light2d.translation != nullptr && light2d.color != nullptr && light2d.shaders != nullptr && light2d.shape != nullptr);

                        // bind_entity_textures allocates the group, binds default at slot 0,
                        // and binds user textures (which may override slot 0 or other slots).
                        auto bound = bind_entity_textures(light2d_light_effect_rend_chain, light2d.textures);

                        // 绑定 G-buffer 通道（漫反射 / 自发光 / 视空间坐标 / 视空间法线 / 阴影）到 4..8 槽
                        jegl_rchain_bind_texture(light2d_light_effect_rend_chain, bound.group, JE_LIGHT2D_DEFER_0 + 0, diffuse_attachment);
                        jegl_rchain_bind_texture(light2d_light_effect_rend_chain, bound.group, JE_LIGHT2D_DEFER_0 + 1, emissive_attachment);
                        jegl_rchain_bind_texture(light2d_light_effect_rend_chain, bound.group, JE_LIGHT2D_DEFER_0 + 2, viewpos_attachment);
                        jegl_rchain_bind_texture(light2d_light_effect_rend_chain, bound.group, JE_LIGHT2D_DEFER_0 + 3, viewnorm_attachment);
                        jegl_rchain_bind_texture(
                            light2d_light_effect_rend_chain, bound.group, JE_LIGHT2D_DEFER_0 + 4,
                            light2d.shadowbuffer != nullptr
                            ? light2d.shadowbuffer->buffer.value()->get_attachment(0).value()->resource()
                            : m_defer_light2d_host._no_shadow->resource());

                        // 开始渲染光照！
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
                        math::vec4 light_color = light2d.color == nullptr ? math::vec4(1.f, 1.f, 1.f, 1.f) : light2d.color->color;
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
                            light2d_light_effect_rend_chain, drawing_shape, drawing_shaders, bound.group, u);
                    }

                    // Rend final result color to screen.
                    jegl_rendchain* final_target_rend_chain = new_rend_chain_for_camera(
                        current_camera.branchPipeline, current_camera.viewport,
                        rend_aim_buffer, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                    clear_color_if_any(final_target_rend_chain, current_camera.clear);
                    jegl_rchain_clear_depth_buffer(final_target_rend_chain, 1.0);
                    jegl_rchain_bind_uniform_buffer(final_target_rend_chain,
                        current_camera.projection->default_uniform_buffer->resource());

                    auto texture_group = jegl_rchain_allocate_texture_group(final_target_rend_chain);

                    // 槽 0：光照合成结果；槽 4..7：G-buffer 通道
                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, 0,
                        current_camera.light2DPostPass->post_light_target.value()->get_attachment(0).value()->resource());
                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, JE_LIGHT2D_DEFER_0 + 0,
                        post_rend_target_frame_buffer->get_attachment(0).value()->resource());
                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, JE_LIGHT2D_DEFER_0 + 1,
                        post_rend_target_frame_buffer->get_attachment(1).value()->resource());
                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, JE_LIGHT2D_DEFER_0 + 2,
                        post_rend_target_frame_buffer->get_attachment(2).value()->resource());
                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, JE_LIGHT2D_DEFER_0 + 3,
                        post_rend_target_frame_buffer->get_attachment(3).value()->resource());

                    // 相机自带纹理（覆盖槽位由 m_pass_id 决定）
                    math::vec2 final_tiling(1.f, 1.f), final_offset(0.f, 0.f);
                    if (current_camera.textures != nullptr)
                    {
                        final_tiling = current_camera.textures->tiling;
                        final_offset = current_camera.textures->offset;
                        for (auto& texture : current_camera.textures->textures)
                            jegl_rchain_bind_texture(final_target_rend_chain, texture_group, texture.m_pass_id, texture.m_texture->resource());
                    }

                    auto& drawing_shaders =
                        current_camera.shaders->shaders.empty() == false
                        ? current_camera.shaders->shaders
                        : m_default_resources.default_shaders_list;

                    math::vec2 post_light_resolution(
                        (float)post_light_target_fb->width(),
                        (float)post_light_target_fb->height());
                    math::vec4 final_color = current_camera.color != nullptr
                        ? current_camera.color->color
                        : math::vec4(1.f, 1.f, 1.f, 1.f);

                    // 注意：合成 pass 不设置 m/mv/mvp/local_scale（这些 shader 不使用它们），
                    // 仅设置 light2d_resolution / tiling / offset / color。
                    for (auto& shader_pass : drawing_shaders)
                    {
                        auto* using_shader = &shader_pass;
                        if (!shader_pass->m_builtin)
                            using_shader = &m_default_resources.default_shader;

                        auto* rchain_draw_action = jegl_rchain_draw(final_target_rend_chain,
                            (*using_shader)->resource(), m_defer_light2d_host._screen_vertex->resource(), texture_group);

                        auto* builtin_uniform = (*using_shader)->m_builtin;

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_resolution, float2,
                            post_light_resolution.x, post_light_resolution.y);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2, final_tiling.x, final_tiling.y);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2, final_offset.x, final_offset.y);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                            final_color.x, final_color.y, final_color.z, final_color.w);
                    }
                } // Finish for Light2d effect.
            }
        }
    };
}
