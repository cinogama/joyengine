#pragma once

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#   error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

#include <queue>
#include <list>

#define JE_LIGHT2D_DEFER_0 4

namespace jeecs
{
#define JE_CHECK_NEED_AND_SET_UNIFORM(ACTION, UNIFORM, ITEM, TYPE, ...)                                      \
    do                                                                                                       \
    {                                                                                                        \
        if (UNIFORM->m_builtin_uniform_##ITEM != graphic::INVALID_UNIFORM_LOCATION)                                     \
            jegl_rchain_set_builtin_uniform_##TYPE(ACTION, &UNIFORM->m_builtin_uniform_##ITEM, __VA_ARGS__); \
    } while (0)

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
            default_shader{ graphic::shader::create("!/builtin/builtin_default.shader", R"(
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
)")
                                 .value() },
            default_texture{ graphic::texture::create(2, 2, jegl_texture::format::RGBA) }, default_shaders_list{ default_shader }
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

        using Translation = Transform::Translation;
        using Rendqueue = Renderer::Rendqueue;

        using PerspectiveProjection = Camera::PerspectiveProjection;
        using OrthoProjection = Camera::OrthoProjection;
        using Projection = Camera::Projection;
        using Viewport = Camera::Viewport;
        using RendToFramebuffer = Camera::RendToFramebuffer;
        using FrustumCulling = Camera::FrustumCulling;

        using Shape = Renderer::Shape;
        using Shaders = Renderer::Shaders;
        using Textures = Renderer::Textures;

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

        void PrepareCameras(
            Transform::Translation& translation,
            Camera::Projection& projection,
            Camera::OrthoProjection* ortho,
            Camera::PerspectiveProjection* perspec,
            Camera::Viewport* viewport,
            Camera::RendToFramebuffer* rendbuf,
            Camera::FrustumCulling* frustumCulling)
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

            size_t
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
                float ortho_height_gain = ortho != nullptr
                    ? graphic::ORTHO_PROJECTION_RATIO / ortho->scale
                    : 1.0f;
                float ortho_width_gain = ortho != nullptr
                    ? (float)RENDAIMBUFFER_WIDTH / (float)RENDAIMBUFFER_HEIGHT * ortho_height_gain
                    : 1.0f;

                float ortho_depth_gain = ortho != nullptr ? projection.zfar * 0.5f : 1.0f;

                // Left clipping plane
                frustumCulling->frustum_plane_normals[0] =
                    ortho_width_gain * math::vec3(
                        projection.view_projection[0][3] + projection.view_projection[0][0],
                        projection.view_projection[1][3] + projection.view_projection[1][0],
                        projection.view_projection[2][3] + projection.view_projection[2][0]);
                frustumCulling->frustum_plane_distance[0] =
                    ortho_width_gain * (projection.view_projection[3][3] + projection.view_projection[3][0]);

                // Right clipping plane
                frustumCulling->frustum_plane_normals[1] =
                    ortho_width_gain * math::vec3(
                        projection.view_projection[0][3] - projection.view_projection[0][0],
                        projection.view_projection[1][3] - projection.view_projection[1][0],
                        projection.view_projection[2][3] - projection.view_projection[2][0]);
                frustumCulling->frustum_plane_distance[1] =
                    ortho_width_gain * (projection.view_projection[3][3] - projection.view_projection[3][0]);

                // Top clipping plane
                frustumCulling->frustum_plane_normals[2] =
                    ortho_height_gain * math::vec3(
                        projection.view_projection[0][3] - projection.view_projection[0][1],
                        projection.view_projection[1][3] - projection.view_projection[1][1],
                        projection.view_projection[2][3] - projection.view_projection[2][1]);
                frustumCulling->frustum_plane_distance[2] =
                    ortho_height_gain * (projection.view_projection[3][3] - projection.view_projection[3][1]);

                // Bottom clipping plane
                frustumCulling->frustum_plane_normals[3] =
                    ortho_height_gain * math::vec3(
                        projection.view_projection[0][3] + projection.view_projection[0][1],
                        projection.view_projection[1][3] + projection.view_projection[1][1],
                        projection.view_projection[2][3] + projection.view_projection[2][1]);
                frustumCulling->frustum_plane_distance[3] =
                    ortho_height_gain * (projection.view_projection[3][3] + projection.view_projection[3][1]);

                // Near clipping plane
                frustumCulling->frustum_plane_normals[4] =
                    ortho_depth_gain * math::vec3(
                        projection.view_projection[0][3] + projection.view_projection[0][2],
                        projection.view_projection[1][3] + projection.view_projection[1][2],
                        projection.view_projection[2][3] + projection.view_projection[2][2]);
                frustumCulling->frustum_plane_distance[4] =
                    ortho_depth_gain * (projection.view_projection[3][3] + projection.view_projection[3][2]);

                // Far clipping plane
                frustumCulling->frustum_plane_normals[5] =
                    ortho_depth_gain * math::vec3(
                        projection.view_projection[0][3] - projection.view_projection[0][2],
                        projection.view_projection[1][3] - projection.view_projection[1][2],
                        projection.view_projection[2][3] - projection.view_projection[2][2]);
                frustumCulling->frustum_plane_distance[5] =
                    ortho_depth_gain * (projection.view_projection[3][3] - projection.view_projection[3][2]);
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

            const auto* const raw_vertex_data =
                light_shape->resource()->m_raw_vertex_data;
            if (raw_vertex_data != nullptr)
            {
                size.x *= 2.0f * std::max(abs(raw_vertex_data->m_x_max), abs(raw_vertex_data->m_x_min));
                size.y *= 2.0f * std::max(abs(raw_vertex_data->m_y_max), abs(raw_vertex_data->m_y_min));
                size.z *= 2.0f * std::max(abs(raw_vertex_data->m_z_max), abs(raw_vertex_data->m_z_min));
            }

            return size;
        }
        math::vec3 get_entity_size(const Transform::Translation& trans, const Renderer::Shape* shape_may_null)
        {
            if (shape_may_null != nullptr)
                return get_entity_size(trans, shape_may_null->vertex);
            else
                return get_entity_size(trans, std::nullopt);
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

        void GraphicUpdate(jeecs::selector& selector)
        {
            auto WINDOWS_SIZE = jeecs::input::windowsize();
            WINDOWS_WIDTH = (size_t)WINDOWS_SIZE.x;
            WINDOWS_HEIGHT = (size_t)WINDOWS_SIZE.y;

            m_camera_list.clear();
            m_renderer_list.clear();

            this->branch_allocate_begin();

            std::unordered_map<typing::uid_t, UserInterface::Origin*> parent_origin_list;

            selector.exec(&UserInterfaceGraphicPipelineSystem::PrepareCameras);

            selector.exec([&](Transform::Anchor& anchor, UserInterface::Origin& origin)
                { parent_origin_list[anchor.uid] = &origin; });

            selector.exec([this](
                Projection& projection,
                Rendqueue* rendqueue,
                Viewport* cameraviewport,
                RendToFramebuffer* rendbuf,
                Camera::Clear* clear)
                {
                    auto* branch = this->allocate_branch(rendqueue == nullptr ? 0 : rendqueue->rend_queue);
                    m_camera_list.insert(
                        camera_arch{
                            branch, rendqueue, &projection, cameraviewport, rendbuf, nullptr, clear
                        }
                    ); });

            selector.anyof<UserInterface::Absolute, UserInterface::Relatively>();
            selector.except<Light2D::Point, Light2D::Parallel, Light2D::Range>();
            selector.exec(
                [this](
                    Shaders& shads,
                    Textures* texs,
                    Shape& shape,
                    Rendqueue* rendqueue,
                    UserInterface::Origin& origin,
                    UserInterface::Rotation* rotation,
                    Renderer::Color* color)
                {
                    m_renderer_list.insert(
                        renderer_arch{
                            color, rendqueue, nullptr, &shape, &shads, texs, &origin, rotation });
                });

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
                assert(current_camera.projection->default_uniform_buffer != nullptr);

                graphic::framebuffer* rend_aim_buffer = nullptr;

                if (current_camera.rendToFramebuffer != nullptr)
                {
                    if (current_camera.rendToFramebuffer->framebuffer.has_value())
                        rend_aim_buffer = current_camera.rendToFramebuffer->framebuffer->get();
                    else
                        continue;
                }
                size_t RENDAIMBUFFER_WIDTH = rend_aim_buffer ? rend_aim_buffer->width() : WINDOWS_WIDTH,
                    RENDAIMBUFFER_HEIGHT = rend_aim_buffer ? rend_aim_buffer->height() : WINDOWS_HEIGHT;

                jegl_rendchain* rend_chain = nullptr;

                const float MAT4_UI_VIEW[4][4] = {
                    {2.0f / (float)RENDAIMBUFFER_WIDTH, 0.0f, 0.0f, 0.0f},
                    {0.0f, 2.0f / (float)RENDAIMBUFFER_HEIGHT, 0.0f, 0.0f},
                    {0.0f, 0.0f, 1.0f, 0.0f},
                    {0.0f, 0.0f, 0.0f, 1.0f},
                };
                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_v_float4x4),
                    sizeof(float) * 16,
                    MAT4_UI_VIEW);
                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_p_float4x4),
                    sizeof(float) * 16,
                    MAT4_UI_UNIT);
                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_vp_float4x4),
                    sizeof(float) * 16,
                    MAT4_UI_VIEW);
                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_time),
                    sizeof(float) * 4,
                    &shader_time);

                if (current_camera.viewport)
                    rend_chain = jegl_branch_new_chain(current_camera.branchPipeline,
                        rend_aim_buffer == nullptr ? nullptr : rend_aim_buffer->resource(),
                        (int32_t)(current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH),
                        (int32_t)(current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT),
                        (uint32_t)(current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH),
                        (uint32_t)(current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT));
                else
                    rend_chain = jegl_branch_new_chain(current_camera.branchPipeline,
                        rend_aim_buffer == nullptr ? nullptr : rend_aim_buffer->resource(),
                        0,
                        0,
                        0,
                        0);

                if (current_camera.clear != nullptr)
                {
                    const float clear_buffer_color[] = {
                        current_camera.clear->color.x,
                        current_camera.clear->color.y,
                        current_camera.clear->color.z,
                        current_camera.clear->color.w };

                    jegl_rchain_clear_color_buffer(rend_chain, 0, clear_buffer_color);
                }

                // Clear depth buffer to overwrite pixels.
                jegl_rchain_clear_depth_buffer(rend_chain, 1.0f);

                jegl_rchain_bind_uniform_buffer(rend_chain,
                    current_camera.projection->default_uniform_buffer->resource());

                // Walk through all entities, rend them to target buffer(include L2DCamera/R2Buf/Screen).
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

                    constexpr jeecs::math::vec2 default_tiling(1.f, 1.f), default_offset(0.f, 0.f);
                    const jeecs::math::vec2
                        * _using_tiling = &default_tiling,
                        * _using_offset = &default_offset;

                    math::vec2 uioffset, uisize, uicenteroffset;
                    rendentity.ui_origin->get_layout(
                        (float)RENDAIMBUFFER_WIDTH,
                        (float)RENDAIMBUFFER_HEIGHT,
                        &uioffset,
                        &uisize,
                        &uicenteroffset);

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

                    auto rchain_texture_group = jegl_rchain_allocate_texture_group(rend_chain);

                    jegl_rchain_bind_texture(rend_chain, rchain_texture_group, 0, m_default_resources.default_texture->resource());
                    if (rendentity.textures)
                    {
                        _using_tiling = &rendentity.textures->tiling;
                        _using_offset = &rendentity.textures->offset;

                        for (auto& texture : rendentity.textures->textures)
                            jegl_rchain_bind_texture(rend_chain, rchain_texture_group, texture.m_pass_id, texture.m_texture->resource());
                    }
                    for (auto& shader_pass : drawing_shaders)
                    {
                        auto* using_shader = &shader_pass;
                        if (!shader_pass->m_builtin)
                            using_shader = &m_default_resources.default_shader;

                        auto* rchain_draw_action = jegl_rchain_draw(
                            rend_chain, (*using_shader)->resource(), drawing_shape->resource(), rchain_texture_group);
                        auto* builtin_uniform = (*using_shader)->m_builtin;

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_UI_MODEL);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_UI_MV);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_UI_MV);

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, local_scale, float3, 1.0f, 1.0f, 1.0f);

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2, _using_tiling->x, _using_tiling->y);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2, _using_offset->x, _using_offset->y);

                        if (rendentity.color != nullptr)
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                rendentity.color->color.x,
                                rendentity.color->color.y,
                                rendentity.color->color.z,
                                rendentity.color->color.w);
                        else
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                1.0f, 1.0f, 1.0f, 1.0f);
                    }
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

        void GraphicUpdate(jeecs::selector& selector)
        {
            auto WINDOWS_SIZE = jeecs::input::windowsize();
            WINDOWS_WIDTH = (size_t)WINDOWS_SIZE.x;
            WINDOWS_HEIGHT = (size_t)WINDOWS_SIZE.y;

            m_camera_list.clear();
            m_renderer_list.clear();

            this->branch_allocate_begin();

            selector.anyof<OrthoProjection, PerspectiveProjection>();
            selector.exec(&UnlitGraphicPipelineSystem::PrepareCameras);

            selector.exec([this](
                Projection& projection,
                Rendqueue* rendqueue,
                Viewport* cameraviewport,
                RendToFramebuffer* rendbuf,
                FrustumCulling* frustumCulling,
                Camera::Clear* clear)
                {
                    auto* branch = this->allocate_branch(rendqueue == nullptr ? 0 : rendqueue->rend_queue);
                    m_camera_list.insert(
                        camera_arch{
                            branch, rendqueue, &projection, cameraviewport, rendbuf, frustumCulling, clear
                        }
                    );
                });

            selector.except<Light2D::Point, Light2D::Parallel, UserInterface::Origin>();
            selector.exec([this](
                Translation& trans,
                Shaders& shads,
                Textures* texs,
                Shape& shape,
                Rendqueue* rendqueue,
                Renderer::Color* color)
                {
                    // RendOb will be input to a chain and used for swap
                    m_renderer_list.insert(
                        renderer_arch{
                            color, rendqueue, &trans, &shape, &shads, texs
                        });
                });

            this->branch_allocate_end();
            DrawFrame();
        }

        void DrawFrame()
        {
            double current_time = je_clock_time();

            math::vec4 shader_time =
            {
                (float)current_time,
                (float)abs(2.0 * (current_time * 2.0 - double(int(current_time * 2.0)) - 0.5)),
                (float)abs(2.0 * (current_time - double(int(current_time)) - 0.5)),
                (float)abs(2.0 * (current_time / 2.0 - double(int(current_time / 2.0)) - 0.5)) };

            float MAT4_MVP[4][4];
            float MAT4_MV[4][4];

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

                size_t
                    RENDAIMBUFFER_WIDTH = rend_aim_buffer ? rend_aim_buffer->width() : WINDOWS_WIDTH,
                    RENDAIMBUFFER_HEIGHT = rend_aim_buffer ? rend_aim_buffer->height() : WINDOWS_HEIGHT;

                const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
                const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;
                const float(&MAT4_VP)[4][4] = current_camera.projection->view_projection;

                assert(current_camera.projection->default_uniform_buffer != nullptr);

                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_v_float4x4),
                    sizeof(float) * 16,
                    MAT4_VIEW);
                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_p_float4x4),
                    sizeof(float) * 16,
                    MAT4_PROJECTION);
                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_vp_float4x4),
                    sizeof(float) * 16,
                    MAT4_VP);
                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_time),
                    sizeof(float) * 4,
                    &shader_time);

                jegl_rendchain* rend_chain = nullptr;

                if (current_camera.viewport)
                    rend_chain = jegl_branch_new_chain(current_camera.branchPipeline,
                        rend_aim_buffer == nullptr ? nullptr : rend_aim_buffer->resource(),
                        (int32_t)(current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH),
                        (int32_t)(current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT),
                        (uint32_t)(current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH),
                        (uint32_t)(current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT));
                else
                    rend_chain = jegl_branch_new_chain(current_camera.branchPipeline,
                        rend_aim_buffer == nullptr ? nullptr : rend_aim_buffer->resource(),
                        0,
                        0,
                        0,
                        0);

                if (current_camera.clear != nullptr)
                {
                    const float clear_buffer_color[] = {
                        current_camera.clear->color.x,
                        current_camera.clear->color.y,
                        current_camera.clear->color.z,
                        current_camera.clear->color.w };
                    jegl_rchain_clear_color_buffer(rend_chain, 0, clear_buffer_color);
                }

                // Clear depth buffer to overwrite pixels.
                jegl_rchain_clear_depth_buffer(rend_chain, 1.0);

                jegl_rchain_bind_uniform_buffer(rend_chain,
                    current_camera.projection->default_uniform_buffer->resource());

                // Walk through all entities, rend them to target buffer(include L2DCamera/R2Buf/Screen).
                for (auto& rendentity : m_renderer_list)
                {
                    assert(rendentity.translation != nullptr && rendentity.shaders != nullptr && rendentity.shape != nullptr);

                    const float entity_range = 0.5f *
                        get_entity_size(*rendentity.translation, rendentity.shape).length();

                    if (current_camera.frustumCulling != nullptr)
                    {
                        if (false == current_camera.frustumCulling->test_circle(
                            rendentity.translation->world_position,
                            entity_range))
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

                    constexpr jeecs::math::vec2 default_tiling(1.f, 1.f), default_offset(0.f, 0.f);
                    const jeecs::math::vec2
                        * _using_tiling = &default_tiling,
                        * _using_offset = &default_offset;

                    assert(rendentity.translation);

                    const float(&MAT4_MODEL)[4][4] = rendentity.translation->object2world;
                    math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                    math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                    auto rchain_texture_group = jegl_rchain_allocate_texture_group(rend_chain);

                    jegl_rchain_bind_texture(rend_chain, rchain_texture_group, 0, m_default_resources.default_texture->resource());
                    if (rendentity.textures)
                    {
                        _using_tiling = &rendentity.textures->tiling;
                        _using_offset = &rendentity.textures->offset;

                        for (auto& texture : rendentity.textures->textures)
                            jegl_rchain_bind_texture(rend_chain, rchain_texture_group, texture.m_pass_id, texture.m_texture->resource());
                    }
                    for (auto& shader_pass : drawing_shaders)
                    {
                        auto* using_shader = &shader_pass;
                        if (!shader_pass->m_builtin)
                            using_shader = &m_default_resources.default_shader;

                        auto* rchain_draw_action = jegl_rchain_draw(rend_chain, (*using_shader)->resource(), drawing_shape->resource(), rchain_texture_group);
                        auto* builtin_uniform = (*using_shader)->m_builtin;

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_MODEL);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_MVP);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_MV);

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, local_scale, float3,
                            rendentity.translation->local_scale.x,
                            rendentity.translation->local_scale.y,
                            rendentity.translation->local_scale.z);

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2, _using_tiling->x, _using_tiling->y);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2, _using_offset->x, _using_offset->y);

                        if (rendentity.color != nullptr)
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                rendentity.color->color.x,
                                rendentity.color->color.y,
                                rendentity.color->color.z,
                                rendentity.color->color.w);
                        else
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                1.0f, 1.0f, 1.0f, 1.0f);
                    }
                }
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

        using Translation = Transform::Translation;

        using Rendqueue = Renderer::Rendqueue;

        using PerspectiveProjection = Camera::PerspectiveProjection;
        using OrthoProjection = Camera::OrthoProjection;
        using Projection = Camera::Projection;
        using Viewport = Camera::Viewport;
        using RendToFramebuffer = Camera::RendToFramebuffer;

        using Shape = Renderer::Shape;
        using Shaders = Renderer::Shaders;
        using Textures = Renderer::Textures;

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

        struct default_uniform_buffer_data_t
        {
            jeecs::math::vec4 time;
        };

        DeferLight2DGraphicPipelineSystem(game_world w)
            : BaseImpledGraphicPipeline(w)
        {
        }

        ~DeferLight2DGraphicPipelineSystem()
        {
        }

        void GraphicUpdate(jeecs::selector& selector)
        {
            auto WINDOWS_SIZE = jeecs::input::windowsize();
            WINDOWS_WIDTH = (size_t)WINDOWS_SIZE.x;
            WINDOWS_HEIGHT = (size_t)WINDOWS_SIZE.y;

            m_2dlight_list.clear();
            m_2dblock_z_list.clear();
            m_2dcamera_list.clear();
            m_renderer_list.clear();

            this->branch_allocate_begin();

            selector.anyof<OrthoProjection, PerspectiveProjection>();
            selector.exec(&DeferLight2DGraphicPipelineSystem::PrepareCameras);

            selector.exec([this](
                Translation& tarns,
                Projection& projection,
                Rendqueue* rendqueue,
                Viewport* cameraviewport,
                RendToFramebuffer* rendbuf,
                Light2D::CameraPostPass* light2dpostpass,
                Shaders* shaders,
                Textures* textures,
                FrustumCulling* frustumCulling,
                Camera::Clear* clear,
                Renderer::Color* color)
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

                        size_t RENDAIMBUFFER_WIDTH =
                            std::max((size_t)1, (size_t)llround(
                                (cameraviewport ? cameraviewport->viewport.z : 1.0f) *
                                (rend_aim_buffer ? rend_aim_buffer->width() : WINDOWS_WIDTH)));
                        size_t RENDAIMBUFFER_HEIGHT =
                            std::max((size_t)1, (size_t)llround(
                                (cameraviewport ? cameraviewport->viewport.w : 1.0f) *
                                (rend_aim_buffer ? rend_aim_buffer->height() : WINDOWS_HEIGHT)));

                        size_t LIGHT_BUFFER_WIDTH =
                            std::max((size_t)1, (size_t)llround(
                                RENDAIMBUFFER_WIDTH * std::max(0.f, std::min(light2dpostpass->light_rend_ratio, 1.0f))));

                        size_t LIGHT_BUFFER_HEIGHT =
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
                });

            selector.except<Light2D::Point, Light2D::Parallel, Light2D::Range, UserInterface::Origin>();
            selector.exec(
                [this](
                    Translation& trans,
                    Shaders& shads,
                    Textures* texs,
                    Shape& shape,
                    Rendqueue* rendqueue,
                    Renderer::Color* color)
                {
                    // RendOb will be input to a chain and used for swap
                    m_renderer_list.insert(
                        renderer_arch{
                            color, rendqueue, &trans, &shape, &shads, texs
                        });
                });

            selector.anyof<Light2D::Point, Light2D::Parallel, Light2D::Range>();
            selector.exec([this](Translation& trans,
                Light2D::TopDown* topdown,
                Light2D::Point* point,
                Light2D::Parallel* parallel,
                Light2D::Range* range,
                Light2D::Gain* gain,
                Light2D::ShadowBuffer* shadowbuffer,
                Renderer::Color* color,
                Shape& shape,
                Shaders& shads,
                Textures* texs)
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
                                [&vertex_datas, &index_datas, &range](const math::vec2& p, size_t layerid)
                                {
                                    vertex_datas.push_back(p.x);
                                    vertex_datas.push_back(p.y);
                                    vertex_datas.push_back(0.f);
                                    vertex_datas.push_back(range->shape.m_strength[layerid]);

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

                                        append_point(range->shape.m_positions.at(last_point_index), 0);
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

                                        append_point(range->shape.m_positions[real_ipoint + ilayer * range->shape.m_point_count], ilayer);

                                        // 如果不是最后一个点，那么链接到下一个顺位点
                                        if (tipoint + 1 != range->shape.m_point_count)
                                            append_point(range->shape.m_positions[real_next_last_layer_ipoint + (ilayer - 1) * range->shape.m_point_count], ilayer - 1);
                                    }

                                    // 最后链接到本层的第一个顺位点
                                    append_point(range->shape.m_positions[last_point_index + ilayer * range->shape.m_point_count], ilayer);
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
                });

            selector.anyof<
                Light2D::BlockShadow,
                Light2D::ShapeShadow,
                Light2D::SpriteShadow,
                Light2D::SelfShadow>();

            selector.exec([this](Translation& trans,
                Light2D::BlockShadow* blockshadow,
                Light2D::ShapeShadow* shapeshadow,
                Light2D::SpriteShadow* spriteshadow,
                Light2D::SelfShadow* selfshadow,
                Textures* texture,
                Shape& shape)
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
                });

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

        void DrawFrame()
        {
            double current_time = je_clock_time();

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

                size_t
                    RENDAIMBUFFER_WIDTH = rend_aim_buffer ? rend_aim_buffer->width() : WINDOWS_WIDTH,
                    RENDAIMBUFFER_HEIGHT = rend_aim_buffer ? rend_aim_buffer->height() : WINDOWS_HEIGHT;

                const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
                const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;
                const float(&MAT4_VP)[4][4] = current_camera.projection->view_projection;

                float MAT4_MV[4][4];
                float MAT4_MVP[4][4];

                assert(current_camera.projection->default_uniform_buffer != nullptr);

                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_v_float4x4),
                    sizeof(float) * 16,
                    MAT4_VIEW);
                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_p_float4x4),
                    sizeof(float) * 16,
                    MAT4_PROJECTION);
                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_vp_float4x4),
                    sizeof(float) * 16,
                    MAT4_VP);
                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_time),
                    sizeof(float) * 4,
                    &shader_time);

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
                                ? get_entity_size(*lightarch.translation, lightarch.shape).length()
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
                            jegl_rendchain* light2d_shadow_rend_chain =
                                jegl_branch_new_chain(
                                    current_camera.branchPipeline,
                                    light2d_shadow_aim_buffer->resource(),
                                    0,
                                    0,
                                    0,
                                    0);

                            const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
                            jegl_rchain_clear_color_buffer(light2d_shadow_rend_chain, 0, clear_color);

                            jegl_rchain_bind_uniform_buffer(light2d_shadow_rend_chain,
                                current_camera.projection->default_uniform_buffer->resource());

                            const auto& normal_shadow_pass =
                                lightarch.parallel != nullptr
                                ? m_defer_light2d_host._defer_light2d_shadow_parallel_pass
                                : m_defer_light2d_host._defer_light2d_shadow_point_pass;
                            const auto& reverse_normal_shadow_pass =
                                lightarch.parallel != nullptr
                                ? m_defer_light2d_host._defer_light2d_shadow_parallel_reverse_pass
                                : m_defer_light2d_host._defer_light2d_shadow_point_reverse_pass;
                            const auto& shape_shadow_pass =
                                lightarch.parallel != nullptr
                                ? m_defer_light2d_host._defer_light2d_shadow_shape_parallel_pass
                                : m_defer_light2d_host._defer_light2d_shadow_shape_point_pass;
                            const auto& sprite_shadow_pass =
                                lightarch.parallel != nullptr
                                ? m_defer_light2d_host._defer_light2d_shadow_sprite_parallel_pass
                                : m_defer_light2d_host._defer_light2d_shadow_sprite_point_pass;
                            const auto& sub_shadow_pass =
                                m_defer_light2d_host._defer_light2d_shadow_sub_pass;

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
                                    get_entity_size(*blockarch.translation, blockarch.shape).length();

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
                                        && (!light_is_above_block || !blockarch.blockshadow->auto_disable))
                                    {
                                        if (blockarch.blockshadow->mesh.m_block_mesh.has_value())
                                        {
                                            auto& using_shadow_pass_shader = blockarch.blockshadow->reverse
                                                ? reverse_normal_shadow_pass
                                                : normal_shadow_pass;

                                            auto* rchain_draw_action = jegl_rchain_draw(
                                                light2d_shadow_rend_chain,
                                                using_shadow_pass_shader->resource(),
                                                blockarch.blockshadow->mesh.m_block_mesh.value()->resource(),
                                                nullptr);

                                            auto* builtin_uniform = using_shadow_pass_shader->m_builtin;

                                            const float(&MAT4_MODEL)[4][4] = blockarch.translation->object2world;
                                            math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                                            math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_MODEL);
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_MV);
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_MVP);

                                            // 通过 local_scale.x 传递阴影权重，.y .z 通道预留
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, local_scale, float3,
                                                blockarch.blockshadow->factor,
                                                0.f,
                                                0.f);

                                            if (lightarch.parallel != nullptr)
                                            {
                                                jeecs::math::vec3 rotated_light_dir =
                                                    lightarch.translation->world_rotation * jeecs::math::vec3(0.f, -1.f, 1.f).unit();
                                                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                                    rotated_light_dir.x,
                                                    rotated_light_dir.y,
                                                    rotated_light_dir.z,
                                                    1.f);
                                            }
                                            else
                                                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                                    lightarch.translation->world_position.x,
                                                    lightarch.translation->world_position.y,
                                                    lightarch.translation->world_position.z,
                                                    1.f);
                                        }
                                    }
                                    if (blockarch.shapeshadow != nullptr
                                        && blockarch.shapeshadow->factor > 0.f
                                        && (light_is_above_block || !blockarch.shapeshadow->auto_disable))
                                    {
                                        auto texture_group = jegl_rchain_allocate_texture_group(light2d_shadow_rend_chain);
                                        if (blockarch.textures != nullptr)
                                        {
                                            auto main_texture = blockarch.textures->get_texture(0);
                                            if (main_texture.has_value())
                                                jegl_rchain_bind_texture(
                                                    light2d_shadow_rend_chain, texture_group, 0, main_texture.value()->resource());
                                            else
                                                jegl_rchain_bind_texture(
                                                    light2d_shadow_rend_chain, texture_group, 0, m_default_resources.default_texture->resource());
                                        }

                                        assert(blockarch.shape != nullptr);
                                        const auto& using_shape = blockarch.shape->vertex.has_value()
                                            ? blockarch.shape->vertex.value()
                                            : m_default_resources.default_shape_quad;

                                        auto* rchain_draw_action = jegl_rchain_draw(
                                            light2d_shadow_rend_chain,
                                            shape_shadow_pass->resource(),
                                            using_shape->resource(),
                                            texture_group);

                                        auto* builtin_uniform = shape_shadow_pass->m_builtin;

                                        const float(&MAT4_MODEL)[4][4] = blockarch.translation->object2world;
                                        math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                                        math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_MODEL);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_MV);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_MVP);

                                        // 通过 local_scale.x 传递阴影权重，.y .z 通道则用于传入tiling_scale参数
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform,
                                            local_scale, float3,
                                            blockarch.shapeshadow->factor,
                                            blockarch.shapeshadow->tiling_scale.x,
                                            blockarch.shapeshadow->tiling_scale.y);

                                        if (blockarch.textures != nullptr)
                                        {
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2,
                                                blockarch.textures->tiling.x, blockarch.textures->tiling.y);
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2,
                                                blockarch.textures->offset.x, blockarch.textures->offset.y);
                                        }

                                        // 通过 je_color 变量传递着色器的位置或方向
                                        if (lightarch.parallel != nullptr)
                                        {
                                            jeecs::math::vec3 rotated_light_dir =
                                                lightarch.translation->world_rotation * jeecs::math::vec3(0.f, -1.f, 0.f);

                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                                rotated_light_dir.x,
                                                rotated_light_dir.y,
                                                rotated_light_dir.z,
                                                blockarch.shapeshadow->distance);
                                        }
                                        else
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                                lightarch.translation->world_position.x,
                                                lightarch.translation->world_position.y,
                                                lightarch.translation->world_position.z,
                                                blockarch.shapeshadow->distance);
                                    }
                                    if (blockarch.spriteshadow != nullptr && blockarch.spriteshadow->factor > 0.f)
                                    {
                                        auto texture_group = jegl_rchain_allocate_texture_group(light2d_shadow_rend_chain);
                                        if (blockarch.textures != nullptr)
                                        {
                                            auto main_texture = blockarch.textures->get_texture(0);
                                            if (main_texture.has_value())
                                                jegl_rchain_bind_texture(
                                                    light2d_shadow_rend_chain, texture_group, 0, main_texture.value()->resource());
                                            else
                                                jegl_rchain_bind_texture(
                                                    light2d_shadow_rend_chain, texture_group, 0, m_default_resources.default_texture->resource());
                                        }

                                        const auto& using_shape = m_defer_light2d_host._sprite_shadow_vertex;

                                        auto* rchain_draw_action = jegl_rchain_draw(
                                            light2d_shadow_rend_chain,
                                            sprite_shadow_pass->resource(),
                                            using_shape->resource(),
                                            texture_group);

                                        auto* builtin_uniform = sprite_shadow_pass->m_builtin;

                                        const float(&MAT4_MODEL)[4][4] = blockarch.translation->object2world;
                                        math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                                        math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_MODEL);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_MV);
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_MVP);

                                        // 通过 local_scale.x 传递阴影权重，.y .z 通道预留
                                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform,
                                            local_scale, float3,
                                            blockarch.spriteshadow->factor,
                                            0.f,
                                            0.f);

                                        if (blockarch.textures != nullptr)
                                        {
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2,
                                                blockarch.textures->tiling.x, blockarch.textures->tiling.y);
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2,
                                                blockarch.textures->offset.x, blockarch.textures->offset.y);
                                        }

                                        // 通过 je_color 变量传递着色器的位置或方向
                                        if (lightarch.parallel != nullptr)
                                        {
                                            jeecs::math::vec3 rotated_light_dir =
                                                lightarch.translation->world_rotation * jeecs::math::vec3(0.f, -1.f, 0.f);

                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                                rotated_light_dir.x,
                                                rotated_light_dir.y,
                                                rotated_light_dir.z,
                                                blockarch.spriteshadow->distance);
                                        }
                                        else
                                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                                lightarch.translation->world_position.x,
                                                lightarch.translation->world_position.y,
                                                lightarch.translation->world_position.z,
                                                blockarch.spriteshadow->distance);
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
                                        auto texture_group = jegl_rchain_allocate_texture_group(light2d_shadow_rend_chain);
                                        if (block_in_layer->textures != nullptr)
                                        {
                                            auto main_texture = block_in_layer->textures->get_texture(0);
                                            if (main_texture.has_value())
                                                jegl_rchain_bind_texture(
                                                    light2d_shadow_rend_chain, texture_group, 0, main_texture.value()->resource());
                                            else
                                                jegl_rchain_bind_texture(
                                                    light2d_shadow_rend_chain, texture_group, 0, m_default_resources.default_texture->resource());
                                        }

                                        assert(block_in_layer->shape != nullptr);
                                        const auto& using_shape =
                                            block_in_layer->shape->vertex.has_value()
                                            ? block_in_layer->shape->vertex.value()
                                            : m_default_resources.default_shape_quad;

                                        // 如果物体被指定为不需要cover，那么就不绘制
                                        if (block_in_layer->selfshadow != nullptr)
                                        {
                                            auto* rchain_draw_action = jegl_rchain_draw(
                                                light2d_shadow_rend_chain,
                                                sub_shadow_pass->resource(),
                                                using_shape->resource(),
                                                texture_group);
                                            auto* builtin_uniform = sub_shadow_pass->m_builtin;

                                            const float(&MAT4_MODEL)[4][4] = block_in_layer->translation->object2world;
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

                    auto light2d_rend_aim_buffer =
                        current_camera.light2DPostPass->post_rend_target.value()->resource();

                    rend_chain = jegl_branch_new_chain(
                        current_camera.branchPipeline,
                        light2d_rend_aim_buffer,
                        0,
                        0,
                        0,
                        0);

                    if (current_camera.clear != nullptr)
                    {
                        const float clear_buffer_color[] = {
                            current_camera.clear->color.x,
                            current_camera.clear->color.y,
                            current_camera.clear->color.z,
                            current_camera.clear->color.w
                        };
                        jegl_rchain_clear_color_buffer(rend_chain, 0, clear_buffer_color);
                    }

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
                    if (current_camera.viewport)
                        rend_chain = jegl_branch_new_chain(current_camera.branchPipeline,
                            rend_aim_buffer == nullptr ? nullptr : rend_aim_buffer->resource(),
                            (int32_t)(current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH),
                            (int32_t)(current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT),
                            (uint32_t)(current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH),
                            (uint32_t)(current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT));
                    else
                        rend_chain = jegl_branch_new_chain(current_camera.branchPipeline,
                            rend_aim_buffer == nullptr ? nullptr : rend_aim_buffer->resource(),
                            0,
                            0,
                            0,
                            0);

                    // If camera rend to texture, clear the frame buffer (if need)
                    if (current_camera.clear != nullptr)
                    {
                        const float clear_buffer_color[] = {
                            current_camera.clear->color.x,
                            current_camera.clear->color.y,
                            current_camera.clear->color.z,
                            current_camera.clear->color.w };
                        jegl_rchain_clear_color_buffer(rend_chain, 0, clear_buffer_color);
                    }
                }
                // Clear depth buffer to overwrite pixels.
                jegl_rchain_clear_depth_buffer(rend_chain, 1.0);

                jegl_rchain_bind_uniform_buffer(rend_chain,
                    current_camera.projection->default_uniform_buffer->resource());

                constexpr jeecs::math::vec2 default_tiling(1.f, 1.f), default_offset(0.f, 0.f);

                // Walk through all entities, rend them to target buffer(include L2DCamera/R2Buf/Screen).
                for (auto& rendentity : m_renderer_list)
                {
                    assert(rendentity.translation != nullptr && rendentity.shaders != nullptr && rendentity.shape != nullptr);

                    const float entity_range = 0.5f *
                        get_entity_size(*rendentity.translation, rendentity.shape).length();

                    if (current_camera.frustumCulling != nullptr)
                    {
                        if (false == current_camera.frustumCulling->test_circle(
                            rendentity.translation->world_position,
                            entity_range))
                            continue;
                    }

                    const float(&MAT4_MODEL)[4][4] = rendentity.translation->object2world;
                    math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                    math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                    const auto& drawing_shape =
                        rendentity.shape->vertex.has_value()
                        ? rendentity.shape->vertex.value()
                        : m_default_resources.default_shape_quad;
                    auto& drawing_shaders =
                        rendentity.shaders->shaders.empty() == false
                        ? rendentity.shaders->shaders
                        : m_default_resources.default_shaders_list;

                    // Bind texture here
                    const jeecs::math::vec2
                        * _using_tiling = &default_tiling,
                        * _using_offset = &default_offset;

                    auto texture_group = jegl_rchain_allocate_texture_group(rend_chain);

                    jegl_rchain_bind_texture(rend_chain, texture_group, 0, m_default_resources.default_texture->resource());
                    if (rendentity.textures)
                    {
                        _using_tiling = &rendentity.textures->tiling;
                        _using_offset = &rendentity.textures->offset;

                        for (auto& texture : rendentity.textures->textures)
                            jegl_rchain_bind_texture(rend_chain, texture_group, texture.m_pass_id, texture.m_texture->resource());
                    }
                    for (auto& shader_pass : drawing_shaders)
                    {
                        auto* using_shader = &shader_pass;
                        if (!shader_pass->m_builtin)
                            using_shader = &m_default_resources.default_shader;

                        auto* rchain_draw_action = jegl_rchain_draw(rend_chain, (*using_shader)->resource(), drawing_shape->resource(), texture_group);

                        auto* builtin_uniform = (*using_shader)->m_builtin;

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_MODEL);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_MV);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_MVP);

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, local_scale, float3,
                            rendentity.translation->local_scale.x,
                            rendentity.translation->local_scale.y,
                            rendentity.translation->local_scale.z);

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2, _using_tiling->x, _using_tiling->y);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2, _using_offset->x, _using_offset->y);

                        if (rendentity.color != nullptr)
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                rendentity.color->color.x,
                                rendentity.color->color.y,
                                rendentity.color->color.z,
                                rendentity.color->color.w);
                        else
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                1.0f, 1.0f, 1.0f, 1.0f);
                    }
                }

                if (current_camera.light2DPostPass != nullptr && current_camera.shaders != nullptr)
                {
                    assert(current_camera.light2DPostPass->post_rend_target.has_value()
                        && current_camera.light2DPostPass->post_light_target.has_value());

                    // Rend Light result to target buffer.
                    jegl_rendchain* light2d_light_effect_rend_chain = jegl_branch_new_chain(
                        current_camera.branchPipeline,
                        current_camera.light2DPostPass->post_light_target.value()->resource(),
                        0,
                        0,
                        0,
                        0);

                    const float clear_color[] = { 0.f, 0.f, 0.f, 0.f };
                    jegl_rchain_clear_color_buffer(
                        light2d_light_effect_rend_chain, 0, clear_color);

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

                        auto texture_group = jegl_rchain_allocate_texture_group(light2d_light_effect_rend_chain);

                        // 绑定漫反射颜色通道
                        jegl_rchain_bind_texture(
                            light2d_light_effect_rend_chain, 
                            texture_group, 
                            JE_LIGHT2D_DEFER_0 + 0,
                            diffuse_attachment);
                        // 绑定自发光通道
                        jegl_rchain_bind_texture(
                            light2d_light_effect_rend_chain, 
                            texture_group, 
                            JE_LIGHT2D_DEFER_0 + 1,
                            emissive_attachment);
                        // 绑定视空间坐标通道
                        jegl_rchain_bind_texture(
                            light2d_light_effect_rend_chain, 
                            texture_group,
                            JE_LIGHT2D_DEFER_0 + 2,
                            viewpos_attachment);
                        // 绑定视空间法线通道
                        jegl_rchain_bind_texture(
                            light2d_light_effect_rend_chain,
                            texture_group, 
                            JE_LIGHT2D_DEFER_0 + 3,
                            viewnorm_attachment);
                        // 绑定阴影
                        jegl_rchain_bind_texture(
                            light2d_light_effect_rend_chain, 
                            texture_group,
                            JE_LIGHT2D_DEFER_0 + 4,
                            light2d.shadowbuffer != nullptr // assert light2d.shadowbuffer->buffer.has_value()
                            ? light2d.shadowbuffer->buffer.value()->get_attachment(0).value()->resource()
                            : m_defer_light2d_host._no_shadow->resource());

                        // 开始渲染光照！
                        const float(&MAT4_MODEL)[4][4] = light2d.translation->object2world;
                        math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                        math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

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

                        // Bind texture here
                        const jeecs::math::vec2
                            * _using_tiling = &default_tiling,
                            * _using_offset = &default_offset;

                        jegl_rchain_bind_texture(
                            light2d_light_effect_rend_chain,
                            texture_group,
                            0,
                            m_default_resources.default_texture->resource());

                        if (light2d.textures != nullptr)
                        {
                            _using_tiling = &light2d.textures->tiling;
                            _using_offset = &light2d.textures->offset;

                            for (auto& texture : light2d.textures->textures)
                                jegl_rchain_bind_texture(
                                    light2d_light_effect_rend_chain,
                                    texture_group,
                                    texture.m_pass_id,
                                    texture.m_texture->resource());
                        }

                        for (auto& shader_pass : drawing_shaders)
                        {
                            auto* using_shader = &shader_pass;
                            if (!shader_pass->m_builtin)
                                using_shader = &m_default_resources.default_shader;

                            auto* rchain_draw_action = jegl_rchain_draw(
                                light2d_light_effect_rend_chain,
                                (*using_shader)->resource(),
                                drawing_shape->resource(),
                                texture_group);
                            auto* builtin_uniform = (*using_shader)->m_builtin;

                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_MODEL);
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_MV);
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_MVP);

                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, local_scale, float3,
                                light2d.translation->local_scale.x,
                                light2d.translation->local_scale.y,
                                light2d.translation->local_scale.z);

                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2,
                                _using_tiling->x, _using_tiling->y);
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2,
                                _using_offset->x, _using_offset->y);

                            // 传入Light2D所需的颜色、衰减信息
                            math::vec4 light_color = light2d.color == nullptr ? math::vec4(1.0f, 1.0f, 1.0f, 1.0f) : light2d.color->color;

                            if (light2d.gain != nullptr)
                                light_color.w *= light2d.gain->gain;

                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                light_color.x,
                                light_color.y,
                                light_color.z,
                                light_color.w);

                            if (light2d.shadowbuffer != nullptr)
                            {
                                size_t SHADOW_BUFFER_WIDTH =
                                    std::max((size_t)1, (size_t)llround(
                                        WINDOWS_WIDTH * std::max(0.f, std::min(light2d.shadowbuffer->resolution_ratio, 1.0f))));

                                size_t SHADOW_BUFFER_HEIGHT =
                                    std::max((size_t)1, (size_t)llround(
                                        WINDOWS_HEIGHT * std::max(0.f, std::min(light2d.shadowbuffer->resolution_ratio, 1.0f))));

                                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_resolution, float2,
                                    (float)SHADOW_BUFFER_WIDTH,
                                    (float)SHADOW_BUFFER_HEIGHT);
                            }
                            else
                                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_resolution, float2,
                                    (float)1.f,
                                    (float)1.f);

                            if (light2d.point != nullptr)
                                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_decay, float,
                                    light2d.point->decay);
                            else if (light2d.range != nullptr)
                                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_decay, float,
                                    light2d.range->decay);
                        }
                    }

                    // Rend final result color to screen.
                    // Set target buffer.
                    jegl_rendchain* final_target_rend_chain = nullptr;
                    if (current_camera.viewport)
                        final_target_rend_chain = jegl_branch_new_chain(current_camera.branchPipeline,
                            rend_aim_buffer == nullptr ? nullptr : rend_aim_buffer->resource(),
                            (int32_t)(current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH),
                            (int32_t)(current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT),
                            (uint32_t)(current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH),
                            (uint32_t)(current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT));
                    else
                        final_target_rend_chain = jegl_branch_new_chain(current_camera.branchPipeline,
                            rend_aim_buffer == nullptr ? nullptr : rend_aim_buffer->resource(),
                            0,
                            0,
                            0,
                            0);

                    if (current_camera.clear != nullptr)
                    {
                        const float clear_buffer_color[] = {
                            current_camera.clear->color.x,
                            current_camera.clear->color.y,
                            current_camera.clear->color.z,
                            current_camera.clear->color.w
                        };
                        jegl_rchain_clear_color_buffer(
                            final_target_rend_chain, 0, clear_buffer_color);
                    }

                    // Clear depth buffer to overwrite pixels.
                    jegl_rchain_clear_depth_buffer(final_target_rend_chain, 1.0);

                    jegl_rchain_bind_uniform_buffer(final_target_rend_chain,
                        current_camera.projection->default_uniform_buffer->resource());

                    auto texture_group = jegl_rchain_allocate_texture_group(final_target_rend_chain);

                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, 0,
                        current_camera.light2DPostPass->post_light_target.value()->get_attachment(0).value()->resource());

                    // 绑定漫反射颜色通道
                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, JE_LIGHT2D_DEFER_0 + 0,
                        post_rend_target_frame_buffer->get_attachment(0).value()->resource());
                    // 绑定自发光通道
                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, JE_LIGHT2D_DEFER_0 + 1,
                        post_rend_target_frame_buffer->get_attachment(1).value()->resource());
                    // 绑定视空间坐标通道
                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, JE_LIGHT2D_DEFER_0 + 2,
                        post_rend_target_frame_buffer->get_attachment(2).value()->resource());
                    // 绑定视空间法线通道
                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, JE_LIGHT2D_DEFER_0 + 3,
                        post_rend_target_frame_buffer->get_attachment(3).value()->resource());

                    const jeecs::math::vec2
                        * _using_tiling = &default_tiling,
                        * _using_offset = &default_offset;

                    if (current_camera.textures)
                    {
                        _using_tiling = &current_camera.textures->tiling;
                        _using_offset = &current_camera.textures->offset;

                        for (auto& texture : current_camera.textures->textures)
                            jegl_rchain_bind_texture(final_target_rend_chain, texture_group, texture.m_pass_id, texture.m_texture->resource());
                    }

                    auto& drawing_shaders =
                        current_camera.shaders->shaders.empty() == false
                        ? current_camera.shaders->shaders
                        : m_default_resources.default_shaders_list;

                    for (auto& shader_pass : drawing_shaders)
                    {
                        auto* using_shader = &shader_pass;
                        if (!shader_pass->m_builtin)
                            using_shader = &m_default_resources.default_shader;

                        auto* rchain_draw_action = jegl_rchain_draw(final_target_rend_chain,
                            (*using_shader)->resource(), m_defer_light2d_host._screen_vertex->resource(), texture_group);

                        auto* builtin_uniform = (*using_shader)->m_builtin;

                        auto* post_light_target_frame_buffer = current_camera.light2DPostPass->post_light_target.value().get();

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_resolution, float2,
                            (float)post_light_target_frame_buffer->width(),
                            (float)post_light_target_frame_buffer->height());

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2, _using_tiling->x, _using_tiling->y);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2, _using_offset->x, _using_offset->y);

                        if (current_camera.color != nullptr)
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                current_camera.color->color.x,
                                current_camera.color->color.y,
                                current_camera.color->color.z,
                                current_camera.color->color.w);
                        else
                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                1.0f, 1.0f, 1.0f, 1.0f);
                    }
                } // Finish for Light2d effect.
            }
        }
    };

    struct FrameAnimationSystem : public game_system
    {
        double _fixed_time = 0.;

        FrameAnimationSystem(game_world w)
            : game_system(w)
        {
        }
        void StateUpdate(jeecs::selector& selector)
        {
            _fixed_time += deltatime();

            selector.exec([this](game_entity e, Animation::FrameAnimation& frame_animation, Renderer::Shaders* shaders)
                {
                    if (abs(frame_animation.speed) == 0.0f)
                        return;

                    for (auto& animation : frame_animation.animations.m_animations)
                    {
                        if (!animation.m_current_action.has_value())
                            continue;

                        auto* active_animation_frames =
                            animation.m_animations.find(animation.m_current_action.value());

                        if (active_animation_frames != animation.m_animations.end())
                        {
                            if (active_animation_frames->v.frames.empty() == false)
                            {
                                // 当前动画数据找到，如果当前帧是 SIZEMAX，或者已经到了要更新帧的时候，
                                if (animation.m_current_frame_index == SIZE_MAX || animation.m_next_update_time <= _fixed_time)
                                {
                                    bool finish_animation = false;

                                    auto update_and_apply_component_frame_data =
                                        [](const game_entity& e, jeecs::Animation::FrameAnimation::animation_list::frame_data& frame)
                                        {
                                            for (auto& cdata : frame.m_component_data)
                                            {
                                                if (cdata.m_entity_cache == e)
                                                    continue;

                                                cdata.m_entity_cache = e;

                                                assert(cdata.m_component_type != nullptr && cdata.m_member_info != nullptr);

                                                auto* component_addr = je_ecs_world_entity_get_component(&e, cdata.m_component_type->m_id);
                                                if (component_addr == nullptr)
                                                    // 没有这个组件，忽略之
                                                    continue;

                                                auto* member_addr = (void*)(cdata.m_member_info->m_member_offset + (intptr_t)component_addr);

                                                // 在这里做好缓存和检查，不要每次都重新获取组件地址和检查类型
                                                cdata.m_member_addr_cache = member_addr;

                                                switch (cdata.m_member_value.m_type)
                                                {
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::INT:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<int>())
                                                    {
                                                        jeecs::debug::logerr(
                                                            "Cannot apply animation frame data for component '%s''s member '%s', "
                                                            "type should be 'int', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::FLOAT:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<float>())
                                                    {
                                                        jeecs::debug::logerr(
                                                            "Cannot apply animation frame data for component '%s''s member '%s', "
                                                            "type should be 'float', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC2:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec2>())
                                                    {
                                                        jeecs::debug::logerr(
                                                            "Cannot apply animation frame data for component '%s''s member '%s', "
                                                            "type should be 'vec2', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC3:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec3>())
                                                    {
                                                        jeecs::debug::logerr(
                                                            "Cannot apply animation frame data for component '%s''s member '%s', "
                                                            "type should be 'vec3', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC4:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec4>())
                                                    {
                                                        jeecs::debug::logerr(
                                                            "Cannot apply animation frame data for component '%s''s member '%s', "
                                                            "type should be 'vec4', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::QUAT4:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::quat>())
                                                    {
                                                        jeecs::debug::logerr(
                                                            "Cannot apply animation frame data for component '%s''s member '%s', "
                                                            "type should be 'quat', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                default:
                                                    jeecs::debug::logerr(
                                                        "Bad animation data type(%d) when trying set data of component '%s''s member '%s', "
                                                        "please check.",
                                                        (int)cdata.m_member_value.m_type,
                                                        cdata.m_component_type->m_typename,
                                                        cdata.m_member_info->m_member_name);
                                                    cdata.m_member_addr_cache = nullptr;
                                                    break;
                                                }
                                            }
                                            for (auto& cdata : frame.m_component_data)
                                            {
                                                if (cdata.m_member_addr_cache == nullptr)
                                                    continue; // Invalid! skip this component.

                                                switch (cdata.m_member_value.m_type)
                                                {
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::INT:
                                                    if (cdata.m_offset_mode)
                                                        *(int*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.i32;
                                                    else
                                                        *(int*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.i32;
                                                    break;
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::FLOAT:
                                                    if (cdata.m_offset_mode)
                                                        *(float*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.f32;
                                                    else
                                                        *(float*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.f32;
                                                    break;
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC2:
                                                    if (cdata.m_offset_mode)
                                                        *(math::vec2*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.v2;
                                                    else
                                                        *(math::vec2*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.v2;
                                                    break;
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC3:
                                                    if (cdata.m_offset_mode)
                                                        *(math::vec3*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.v3;
                                                    else
                                                        *(math::vec3*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.v3;
                                                    break;
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC4:
                                                    if (cdata.m_offset_mode)
                                                        *(math::vec4*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.v4;
                                                    else
                                                        *(math::vec4*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.v4;
                                                    break;
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::QUAT4:
                                                    if (cdata.m_offset_mode)
                                                        *(math::quat*)cdata.m_member_addr_cache =
                                                        *(math::quat*)cdata.m_member_addr_cache
                                                        * cdata.m_member_value.m_value.q4;
                                                    else
                                                        *(math::quat*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.q4;
                                                    break;
                                                default:
                                                    jeecs::debug::logerr(
                                                        "Bad animation data type(%d) when trying set data of component '%s''s member '%s', "
                                                        "please check.",
                                                        (int)cdata.m_member_value.m_type,
                                                        cdata.m_component_type->m_typename,
                                                        cdata.m_member_info->m_member_name);
                                                    break;
                                                }
                                            }
                                        };

                                    auto current_animation_frame_count = active_animation_frames->v.frames.size();

                                    if (animation.m_current_frame_index == SIZE_MAX || animation.m_last_speed != frame_animation.speed)
                                    {
                                        animation.m_current_frame_index = 0;
                                        animation.m_next_update_time =
                                            _fixed_time
                                            + (active_animation_frames->v.frames[animation.m_current_frame_index].m_frame_time
                                                + math::random(-frame_animation.jitter, frame_animation.jitter))
                                            / frame_animation.speed;
                                    }
                                    else
                                    {
                                        // 到达下一次更新时间！检查间隔时间，并跳转到对应的帧
                                        auto delta_time_between_frams = _fixed_time - animation.m_next_update_time;
                                        auto next_frame_index = (animation.m_current_frame_index + 1) % current_animation_frame_count;

                                        while (
                                            delta_time_between_frams > active_animation_frames->v.frames[next_frame_index].m_frame_time / frame_animation.speed)
                                        {
                                            if (animation.m_loop == false && next_frame_index == current_animation_frame_count - 1)
                                                break;

                                            // 在此应用跳过帧的deltaframe数据
                                            update_and_apply_component_frame_data(e, active_animation_frames->v.frames[next_frame_index]);

                                            delta_time_between_frams -=
                                                active_animation_frames->v.frames[next_frame_index].m_frame_time / frame_animation.speed;
                                            next_frame_index = (next_frame_index + 1) % current_animation_frame_count;
                                        }

                                        animation.m_current_frame_index = next_frame_index;
                                        animation.m_next_update_time =
                                            _fixed_time
                                            - delta_time_between_frams
                                            + (active_animation_frames->v.frames[animation.m_current_frame_index].m_frame_time
                                                + math::random(-frame_animation.jitter, frame_animation.jitter))
                                            / frame_animation.speed;
                                    }

                                    if (animation.m_loop == false)
                                    {
                                        if (animation.m_current_frame_index == current_animation_frame_count - 1)
                                            finish_animation = true;
                                    }

                                    auto& updating_frame = active_animation_frames->v.frames[animation.m_current_frame_index];
                                    update_and_apply_component_frame_data(e, updating_frame);

                                    if (shaders != nullptr)
                                    {
                                        for (auto& udata : updating_frame.m_uniform_data)
                                        {
                                            switch (udata.m_uniform_value.m_type)
                                            {
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::INT:
                                                shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.i32);
                                                break;
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::FLOAT:
                                                shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.f32);
                                                break;
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC2:
                                                shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.v2);
                                                break;
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC3:
                                                shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.v3);
                                                break;
                                            case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC4:
                                                shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.v4);
                                                break;
                                            default:
                                                jeecs::debug::logerr(
                                                    "Bad animation data type(%d) when trying set data of uniform variable '%s', "
                                                    "please check.",
                                                    (int)udata.m_uniform_value.m_type,
                                                    udata.m_uniform_name.c_str());
                                                break;
                                            }
                                        }
                                    }
                                    if (finish_animation)
                                    {
                                        // 终止动画
                                        animation.stop();
                                    }
                                    animation.m_last_speed = frame_animation.speed;
                                }
                            }
                        }
                        else
                        {
                            // 如果没有找到对应的动画，那么终止动画
                            animation.stop();
                        }
                        // 这个注释写在这里单纯是因为花括号写得太难看，稍微避免出现一个大于号
                    }
                    // End
                });
        }
    };
}
