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

namespace jeecs
{
#define JE_CHECK_NEED_AND_SET_UNIFORM(ACTION, UNIFORM, ITEM, TYPE, ...) \
do{if (UNIFORM->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
    jegl_rchain_set_builtin_uniform_##TYPE(ACTION, &UNIFORM->m_builtin_uniform_##ITEM, __VA_ARGS__);}while(0)

    struct DefaultResources
    {
        basic::resource<graphic::vertex> default_shape_quad;
        basic::resource<graphic::shader> default_shader;
        basic::resource<graphic::texture> default_texture;
        basic::vector<basic::resource<graphic::shader>> default_shaders_list;

        JECS_DISABLE_MOVE_AND_COPY(DefaultResources);

        DefaultResources()
        {
            const float default_shape_quad_data[] = {
                -0.5f, 0.5f, 0.0f,      0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
                -0.5f, -0.5f, 0.0f,     0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
                0.5f, 0.5f, 0.0f,       1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
                0.5f, -0.5f, 0.0f,      1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
            };
            default_shape_quad =
                graphic::vertex::create(jegl_vertex::TRIANGLESTRIP,
                    default_shape_quad_data,
                    sizeof(default_shape_quad_data),
                    {
                        0, 1, 2, 3
                    },
                    {
                        {jegl_vertex::data_type::FLOAT32, 3},
                        {jegl_vertex::data_type::FLOAT32, 2},
                        {jegl_vertex::data_type::FLOAT32, 3},
                    });

                    default_texture = graphic::texture::create(2, 2, jegl_texture::format::RGBA);
                    default_texture->pix(0, 0).set({ 1.f, 0.25f, 1.f, 1.f });
                    default_texture->pix(1, 1).set({ 1.f, 0.25f, 1.f, 1.f });
                    default_texture->pix(0, 1).set({ 0.25f, 0.25f, 0.25f, 1.f });
                    default_texture->pix(1, 0).set({ 0.25f, 0.25f, 0.25f, 1.f });

                    default_shader = graphic::shader::create("!/builtin/builtin_default.shader", R"(
// Default shader
import je::shader;

VAO_STRUCT! vin {
    vertex : float3,
};
using v2f = struct {
    pos : float4,
};
using fout = struct {
    color : float4
};

public let vert = 
\v: vin = v2f{ pos = je_mvp * vertex_pos }
    where vertex_pos = vec4(v.vertex, 1.);;

public let frag = 
\_: v2f = fout{ color = vec4(t, 0., t, 1.) }
    where t = je_time->y();;

)");
                    default_shaders_list.push_back(default_shader);
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

            const Renderer::Rendqueue* rendqueue;

            const Camera::Projection* projection;
            const Camera::Viewport* viewport;
            const Camera::RendToFramebuffer* rendToFramebuffer;
            const Camera::FrustumCulling* frustumCulling;
            const Camera::Clear* clear;

            bool operator < (const camera_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };
        struct renderer_arch
        {
            const Transform::Translation* translation;

            const UserInterface::Origin* ui_origin;
            const UserInterface::Rotation* ui_rotation;

            const Renderer::Color* color;
            const Renderer::Rendqueue* rendqueue;
            const Renderer::Shape* shape;
            const Renderer::Shaders* shaders;
            const Renderer::Textures* textures;

            bool operator < (const renderer_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };

        DefaultResources m_default_resources;

        std::vector<camera_arch> m_camera_list;
        std::vector<renderer_arch> m_renderer_list;

        size_t WINDOWS_WIDTH = 0;
        size_t WINDOWS_HEIGHT = 0;

        void PrepareCameras(
            Transform::Translation& translation,
            Camera::Projection& projection,
            Camera::OrthoProjection* ortho,
            Camera::PerspectiveProjection* perspec,
            Camera::Clip* clip,
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
            float znear = clip ? clip->znear : 0.3f;
            float zfar = clip ? clip->zfar : 1000.0f;

            graphic::framebuffer* rend_aim_buffer =
                rendbuf && rendbuf->framebuffer ? rendbuf->framebuffer.get() : nullptr;

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
                    ortho->scale, znear, zfar);
                graphic::ortho_inv_projection(projection.inv_projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    ortho->scale, znear, zfar);
            }
            else
            {
                graphic::perspective_projection(projection.projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    perspec->angle, znear, zfar);
                graphic::perspective_inv_projection(projection.inv_projection,
                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
                    perspec->angle, znear, zfar);
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

                float ortho_depth_gain = ortho != nullptr ? zfar * 0.5f : 1.0f;

                // Left clipping plane
                frustumCulling->frustum_plane_normals[0] =
                    ortho_width_gain * math::vec3(
                        projection.view_projection[0][3] + projection.view_projection[0][0],
                        projection.view_projection[1][3] + projection.view_projection[1][0],
                        projection.view_projection[2][3] + projection.view_projection[2][0]
                    );
                frustumCulling->frustum_plane_distance[0] =
                    ortho_width_gain * (projection.view_projection[3][3] + projection.view_projection[3][0]);

                // Right clipping plane
                frustumCulling->frustum_plane_normals[1] =
                    ortho_width_gain * math::vec3(
                        projection.view_projection[0][3] - projection.view_projection[0][0],
                        projection.view_projection[1][3] - projection.view_projection[1][0],
                        projection.view_projection[2][3] - projection.view_projection[2][0]
                    );
                frustumCulling->frustum_plane_distance[1] =
                    ortho_width_gain * (projection.view_projection[3][3] - projection.view_projection[3][0]);

                // Top clipping plane
                frustumCulling->frustum_plane_normals[2] =
                    ortho_height_gain * math::vec3(
                        projection.view_projection[0][3] - projection.view_projection[0][1],
                        projection.view_projection[1][3] - projection.view_projection[1][1],
                        projection.view_projection[2][3] - projection.view_projection[2][1]
                    );
                frustumCulling->frustum_plane_distance[2] =
                    ortho_height_gain * (projection.view_projection[3][3] - projection.view_projection[3][1]);

                // Bottom clipping plane
                frustumCulling->frustum_plane_normals[3] =
                    ortho_height_gain * math::vec3(
                        projection.view_projection[0][3] + projection.view_projection[0][1],
                        projection.view_projection[1][3] + projection.view_projection[1][1],
                        projection.view_projection[2][3] + projection.view_projection[2][1]
                    );
                frustumCulling->frustum_plane_distance[3] =
                    ortho_height_gain * (projection.view_projection[3][3] + projection.view_projection[3][1]);

                // Near clipping plane
                frustumCulling->frustum_plane_normals[4] =
                    ortho_depth_gain * math::vec3(
                        projection.view_projection[0][3] + projection.view_projection[0][2],
                        projection.view_projection[1][3] + projection.view_projection[1][2],
                        projection.view_projection[2][3] + projection.view_projection[2][2]
                    );
                frustumCulling->frustum_plane_distance[4] =
                    ortho_depth_gain * (projection.view_projection[3][3] + projection.view_projection[3][2]);

                // Far clipping plane
                frustumCulling->frustum_plane_normals[5] =
                    ortho_depth_gain * math::vec3(
                        projection.view_projection[0][3] - projection.view_projection[0][2],
                        projection.view_projection[1][3] - projection.view_projection[1][2],
                        projection.view_projection[2][3] - projection.view_projection[2][2]
                    );
                frustumCulling->frustum_plane_distance[5] =
                    ortho_depth_gain * (projection.view_projection[3][3] - projection.view_projection[3][2]);
            }
        }

        math::vec3 get_entity_size(const Transform::Translation& trans, const basic::resource<graphic::vertex>& mesh)
        {
            math::vec3 size = trans.local_scale;

            const auto& light_shape = mesh != nullptr
                ? mesh
                : m_default_resources.default_shape_quad;

            assert(light_shape->resouce() != nullptr);

            const auto* const raw_vertex_data =
                light_shape->resouce()->m_raw_vertex_data;
            if (raw_vertex_data != nullptr)
            {
                size.x *= 2.0f * std::max(abs(raw_vertex_data->m_x_max), abs(raw_vertex_data->m_x_min));
                size.y *= 2.0f * std::max(abs(raw_vertex_data->m_y_max), abs(raw_vertex_data->m_y_min));
                size.z *= 2.0f * std::max(abs(raw_vertex_data->m_z_max), abs(raw_vertex_data->m_z_min));
            }

            return size;
        }
        math::vec3 get_entity_size(const Transform::Translation& trans, const Renderer::Shape& shape)
        {
            return get_entity_size(trans, shape.vertex);
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
                {
                    parent_origin_list[anchor.uid] = &origin;
                }
            );

            selector.exec(
                [this](
                    Renderer::Rendqueue* rendqueue,
                    Camera::Projection& projection,
                    Camera::Viewport* cameraviewport,
                    Camera::RendToFramebuffer* rendbuf,
                    Camera::Clear* clear)
                {
                    auto* branch = this->allocate_branch(rendqueue == nullptr ? 0 : rendqueue->rend_queue);
                    m_camera_list.emplace_back(
                        camera_arch{
                            branch, rendqueue, &projection, cameraviewport, rendbuf, nullptr, clear
                        }
                    );
                });

            selector.anyof<UserInterface::Absolute, UserInterface::Relatively>();
            selector.except<Light2D::Point, Light2D::Parallel, Light2D::Range>();
            selector.exec(
                [this, &parent_origin_list](
                    UserInterface::Origin& origin,
                    UserInterface::Rotation* rotation,
                    Renderer::Shaders& shads,
                    Renderer::Textures* texs,
                    Renderer::Shape& shape,
                    Renderer::Rendqueue* rendqueue,
                    Renderer::Color* color)
                {
                    m_renderer_list.emplace_back(
                        renderer_arch{
                            nullptr, 
                            &origin, 
                            rotation, 
                            color, 
                            rendqueue, 
                            &shape, 
                            &shads, 
                            texs,
                        });
                });
            ;
            std::sort(m_camera_list.begin(), m_camera_list.end());
            std::sort(m_renderer_list.begin(), m_renderer_list.end());

            this->branch_allocate_end();

            DrawFrame();
        }

        void DrawFrame()
        {
            if (WINDOWS_WIDTH == 0 || WINDOWS_HEIGHT == 0)
                // Windows' size is invalid, skip this frame.
                return;

            // TODO: Update shared uniform.
            double current_time = je_clock_time();

            math::vec4 shader_time =
            {
                (float)current_time ,
                (float)abs(2.0 * (current_time * 2.0 - double(int(current_time * 2.0)) - 0.5)) ,
                (float)abs(2.0 * (current_time - double(int(current_time)) - 0.5)),
                (float)abs(2.0 * (current_time / 2.0 - double(int(current_time / 2.0)) - 0.5))
            };

            const float MAT4_UI_UNIT[4][4] = {
                { 1.0f, 0.0f, 0.0f, 0.0f },
                { 0.0f, 1.0f, 0.0f, 0.0f },
                { 0.0f, 0.0f, 1.0f, 0.0f },
                { 0.0f, 0.0f, 0.0f, 1.0f },
            };

            float MAT4_UI_MODEL[4][4];
            float MAT4_UI_MV[4][4];

            for (auto& current_camera : m_camera_list)
            {
                assert(current_camera.projection->default_uniform_buffer != nullptr);

                graphic::framebuffer* rend_aim_buffer = nullptr;

                if (current_camera.rendToFramebuffer)
                {
                    if (current_camera.rendToFramebuffer->framebuffer == nullptr)
                        continue;
                    else
                        rend_aim_buffer = current_camera.rendToFramebuffer->framebuffer.get();
                }

                size_t
                    RENDAIMBUFFER_WIDTH = rend_aim_buffer ? rend_aim_buffer->width() : WINDOWS_WIDTH,
                    RENDAIMBUFFER_HEIGHT = rend_aim_buffer ? rend_aim_buffer->height() : WINDOWS_HEIGHT;

                jegl_rendchain* rend_chain = nullptr;

                const float MAT4_UI_VIEW[4][4] = {
                       { 2.0f / (float)RENDAIMBUFFER_WIDTH , 0.0f, 0.0f, 0.0f },
                       { 0.0f, 2.0f / (float)RENDAIMBUFFER_HEIGHT, 0.0f, 0.0f },
                       { 0.0f, 0.0f, 1.0f, 0.0f },
                       { 2.0f / (float)RENDAIMBUFFER_WIDTH, 2.0f / (float)RENDAIMBUFFER_HEIGHT, 0.0f, 1.0f },
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
                        rend_aim_buffer == nullptr ? nullptr : rend_aim_buffer->resouce(),
                        (size_t)(current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH),
                        (size_t)(current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT),
                        (size_t)(current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH),
                        (size_t)(current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT));
                else
                    rend_chain = jegl_branch_new_chain(current_camera.branchPipeline,
                        rend_aim_buffer == nullptr ? nullptr : rend_aim_buffer->resouce(),
                        0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                if (current_camera.clear != nullptr)
                {
                    float clear_buffer_color[] = {
                        current_camera.clear->color.x,
                        current_camera.clear->color.y,
                        current_camera.clear->color.z,
                        current_camera.clear->color.w
                    };
                    jegl_rchain_clear_color_buffer(rend_chain, clear_buffer_color);
                }

                // Clear depth buffer to overwrite pixels.
                jegl_rchain_clear_depth_buffer(rend_chain);

                jegl_rchain_bind_uniform_buffer(rend_chain,
                    current_camera.projection->default_uniform_buffer->resouce());

                // Walk through all entities, rend them to target buffer(include L2DCamera/R2Buf/Screen).
                for (auto& rendentity : m_renderer_list)
                {
                    assert(rendentity.ui_origin != nullptr
                        && rendentity.shaders != nullptr
                        && rendentity.shape != nullptr);

                    auto& drawing_shape =
                        rendentity.shape->vertex != nullptr
                        ? rendentity.shape->vertex
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
                        (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT, &uioffset, &uisize, &uicenteroffset);

                    uioffset.x -= (float)RENDAIMBUFFER_WIDTH / 2.0f;
                    uioffset.y -= (float)RENDAIMBUFFER_HEIGHT / 2.0f;

                    // TODO: 这里俩矩阵实际上可以优化，但是UI实际上也没有多少，暂时直接矩阵乘法也无所谓
                    // NOTE: 这里的大小和偏移大小乘二是因为一致空间是 -1 到 1，天然有一个1/2的压缩，为了保证单位正确，这里乘二
                    const float MAT4_UI_OFFSET[4][4] = {
                        { 1.0f, 0.0f, 0.0f, 0.0f },
                        { 0.0f, 1.0f, 0.0f, 0.0f },
                        { 0.0f, 0.0f, 1.0f, 0.0f },
                        { uioffset.x, uioffset.y, 0.0f, 1.0f },
                    };
                    const float MAT4_UI_SIZE[4][4] = {
                        {uisize.x, 0.0f, 0.0f, 0.0f},
                        {0.0f, uisize.y, 0.0f, 0.0f},
                        {0.0f, 0.0f, 1.0f, 0.0f},
                        {0.0f, 0.0f, 0.0f, 1.0f}
                    };

                    float MAT4_UI_ROTATION[4][4] = {
                        { 1.0f, 0.0f, 0.0f, 0.0f },
                        { 0.0f, 1.0f, 0.0f, 0.0f },
                        { 0.0f, 0.0f, 1.0f, 0.0f },
                        { 0.0f, 0.0f, 0.0f, 1.0f },
                    };
                    if (rendentity.ui_rotation != nullptr)
                    {
                        const float MAT4_UI_CENTER_OFFSET[4][4] = {
                            {1.0f, 0.0f, 0.0f, 0.0f},
                            {0.0f, 1.0f, 0.0f, 0.0f},
                            {0.0f, 0.0f, 1.0f, 0.0f},
                            {uicenteroffset.x, uicenteroffset.y, 0.0f, 1.0f}
                        };
                        const float MAT4_UI_INV_CENTER_OFFSET[4][4] = {
                            {1.0f, 0.0f, 0.0f, 0.0f},
                            {0.0f, 1.0f, 0.0f, 0.0f},
                            {0.0f, 0.0f, 1.0f, 0.0f},
                            {-uicenteroffset.x, -uicenteroffset.y, 0.0f, 1.0f}
                        };

                        math::quat q(0.0f, 0.0f, rendentity.ui_rotation->angle);
                        q.create_matrix(MAT4_UI_ROTATION);

                        math::mat4xmat4(MAT4_UI_MV/* tmp */, MAT4_UI_ROTATION, MAT4_UI_CENTER_OFFSET);
                        math::mat4xmat4(MAT4_UI_ROTATION, MAT4_UI_INV_CENTER_OFFSET, MAT4_UI_MV/* tmp */);
                    }
                    math::mat4xmat4(MAT4_UI_MV/* tmp */, MAT4_UI_OFFSET, MAT4_UI_ROTATION);
                    math::mat4xmat4(MAT4_UI_MODEL, MAT4_UI_MV/* tmp */, MAT4_UI_SIZE);
                    math::mat4xmat4(MAT4_UI_MV, MAT4_UI_VIEW, MAT4_UI_MODEL);

                    auto rchain_texture_group = jegl_rchain_allocate_texture_group(rend_chain);

                    jegl_rchain_bind_texture(rend_chain, rchain_texture_group, 0, m_default_resources.default_texture->resouce());
                    if (rendentity.textures)
                    {
                        _using_tiling = &rendentity.textures->tiling;
                        _using_offset = &rendentity.textures->offset;

                        for (auto& texture : rendentity.textures->textures)
                            jegl_rchain_bind_texture(rend_chain, rchain_texture_group, texture.m_pass_id, texture.m_texture->resouce());
                    }
                    for (auto& shader_pass : drawing_shaders)
                    {
                        auto* using_shader = &shader_pass;
                        if (!shader_pass->m_builtin)
                            using_shader = &m_default_resources.default_shader;

                        auto* rchain_draw_action = jegl_rchain_draw(rend_chain, (*using_shader)->resouce(), drawing_shape->resouce(), rchain_texture_group);
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

    struct DefaultGraphicPipelineSystem : public BaseImpledGraphicPipeline
    {
        DefaultGraphicPipelineSystem(game_world w)
            : BaseImpledGraphicPipeline(w)
        {
        }

        ~DefaultGraphicPipelineSystem()
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

            selector.anyof<Camera::OrthoProjection, Camera::PerspectiveProjection>();
            selector.exec(&DefaultGraphicPipelineSystem::PrepareCameras);

            selector.exec([this](
                Renderer::Rendqueue* rendqueue,
                Camera::Projection& projection,
                Camera::Viewport* cameraviewport,
                Camera::RendToFramebuffer* rendbuf,
                Camera::FrustumCulling* frustumCulling,
                Camera::Clear* clear)
                {
                    auto* branch = this->allocate_branch(rendqueue == nullptr ? 0 : rendqueue->rend_queue);
                    m_camera_list.emplace_back(
                        camera_arch{
                            branch, 
                            rendqueue, 
                            &projection, 
                            cameraviewport, 
                            rendbuf, 
                            frustumCulling, 
                            clear
                        }
                    );
                });

            selector.except<Light2D::Point, Light2D::Parallel, UserInterface::Origin>();
            selector.exec([this](
                Transform::Translation& trans,
                Renderer::Shaders& shads,
                Renderer::Textures* texs,
                Renderer::Shape& shape,
                Renderer::Rendqueue* rendqueue,
                Renderer::Color* color)
                {
                    // TODO: Need Impl AnyOf
                        // RendOb will be input to a chain and used for swap
                    m_renderer_list.emplace_back(
                        renderer_arch{
                            &trans, 
                            nullptr, 
                            nullptr,
                            color, 
                            rendqueue, 
                            &shape, 
                            &shads, 
                            texs
                        });
                });

            std::sort(m_camera_list.begin(), m_camera_list.end());
            std::sort(m_renderer_list.begin(), m_renderer_list.end());

            this->branch_allocate_end();

            DrawFrame();
        }

        void DrawFrame()
        {
            if (WINDOWS_WIDTH == 0 || WINDOWS_HEIGHT == 0)
                // Windows' size is invalid, skip this frame.
                return;

            // TODO: Update shared uniform.
            double current_time = je_clock_time();

            math::vec4 shader_time =
            {
                (float)current_time ,
                (float)abs(2.0 * (current_time * 2.0 - double(int(current_time * 2.0)) - 0.5)) ,
                (float)abs(2.0 * (current_time - double(int(current_time)) - 0.5)),
                (float)abs(2.0 * (current_time / 2.0 - double(int(current_time / 2.0)) - 0.5))
            };

            for (auto& current_camera : m_camera_list)
            {
                graphic::framebuffer* rend_aim_buffer = nullptr;
                if (current_camera.rendToFramebuffer)
                {
                    if (current_camera.rendToFramebuffer->framebuffer == nullptr)
                        continue;
                    else
                        rend_aim_buffer = current_camera.rendToFramebuffer->framebuffer.get();
                }

                size_t
                    RENDAIMBUFFER_WIDTH = rend_aim_buffer ? rend_aim_buffer->width() : WINDOWS_WIDTH,
                    RENDAIMBUFFER_HEIGHT = rend_aim_buffer ? rend_aim_buffer->height() : WINDOWS_HEIGHT;

                const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
                const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;
                const float(&MAT4_VP)[4][4] = current_camera.projection->view_projection;

                float MAT4_MV[4][4];

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
                        rend_aim_buffer == nullptr ? nullptr : rend_aim_buffer->resouce(),
                        (size_t)(current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH),
                        (size_t)(current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT),
                        (size_t)(current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH),
                        (size_t)(current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT));
                else
                    rend_chain = jegl_branch_new_chain(current_camera.branchPipeline,
                        rend_aim_buffer == nullptr ? nullptr : rend_aim_buffer->resouce(),
                        0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                if (current_camera.clear != nullptr)
                {
                    float clear_buffer_color[] = {
                        current_camera.clear->color.x,
                        current_camera.clear->color.y,
                        current_camera.clear->color.z,
                        current_camera.clear->color.w
                    };
                    jegl_rchain_clear_color_buffer(rend_chain, clear_buffer_color);
                }

                // Clear depth buffer to overwrite pixels.
                jegl_rchain_clear_depth_buffer(rend_chain);

                jegl_rchain_bind_uniform_buffer(rend_chain,
                    current_camera.projection->default_uniform_buffer->resouce());

                // Walk through all entities, rend them to target buffer(include L2DCamera/R2Buf/Screen).
                for (auto& rendentity : m_renderer_list)
                {
                    assert(rendentity.translation != nullptr
                        && rendentity.shaders != nullptr
                        && rendentity.shape != nullptr);

                    const float entity_range = 0.5f *
                        get_entity_size(*rendentity.translation, *rendentity.shape).length();

                    if (current_camera.frustumCulling != nullptr)
                    {
                        if (false == current_camera.frustumCulling->test_circle(
                            rendentity.translation->world_position,
                            entity_range))
                            continue;
                    }

                    auto& drawing_shape =
                        rendentity.shape->vertex != nullptr
                        ? rendentity.shape->vertex
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

                    float MAT4_MVP[4][4];
                    const float(&MAT4_MODEL)[4][4] = rendentity.translation->object2world;
                    math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                    math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                    auto rchain_texture_group = jegl_rchain_allocate_texture_group(rend_chain);

                    jegl_rchain_bind_texture(rend_chain, rchain_texture_group, 0, m_default_resources.default_texture->resouce());
                    if (rendentity.textures)
                    {
                        _using_tiling = &rendentity.textures->tiling;
                        _using_offset = &rendentity.textures->offset;

                        for (auto& texture : rendentity.textures->textures)
                            jegl_rchain_bind_texture(rend_chain, rchain_texture_group, texture.m_pass_id, texture.m_texture->resouce());
                    }
                    for (auto& shader_pass : drawing_shaders)
                    {
                        auto* using_shader = &shader_pass;
                        if (!shader_pass->m_builtin)
                            using_shader = &m_default_resources.default_shader;

                        auto* rchain_draw_action = jegl_rchain_draw(rend_chain, (*using_shader)->resouce(), drawing_shape->resouce(), rchain_texture_group);
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
                        auto* active_animation_frames =
                            animation.m_animations.find(animation.m_current_action);

                        if (active_animation_frames != animation.m_animations.end())
                        {
                            if (active_animation_frames->v.frames.empty() == false)
                            {
                                // 当前动画数据找到，如果当前帧是 SIZEMAX，或者已经到了要更新帧的时候，
                                if (animation.m_current_frame_index == SIZE_MAX
                                    || animation.m_next_update_time <= _fixed_time)
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
                                                        jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'int', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::FLOAT:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<float>())
                                                    {
                                                        jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'float', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC2:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec2>())
                                                    {
                                                        jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'vec2', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC3:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec3>())
                                                    {
                                                        jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'vec3', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::VEC4:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec4>())
                                                    {
                                                        jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'vec4', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                case Animation::FrameAnimation::animation_list::frame_data::data_value::type::QUAT4:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::quat>())
                                                    {
                                                        jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'quat', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                default:
                                                    jeecs::debug::logerr("Bad animation data type(%d) when trying set data of component '%s''s member '%s', please check.",
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
                                                        *(math::quat*)cdata.m_member_addr_cache = *(math::quat*)cdata.m_member_addr_cache * cdata.m_member_value.m_value.q4;
                                                    else
                                                        *(math::quat*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.q4;
                                                    break;
                                                default:
                                                    jeecs::debug::logerr("Bad animation data type(%d) when trying set data of component '%s''s member '%s', please check.",
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
                                            + active_animation_frames->v.frames[animation.m_current_frame_index].m_frame_time / frame_animation.speed
                                            + math::random(-frame_animation.jitter, frame_animation.jitter) / frame_animation.speed;
                                    }
                                    else
                                    {
                                        // 到达下一次更新时间！检查间隔时间，并跳转到对应的帧
                                        auto delta_time_between_frams = _fixed_time - animation.m_next_update_time;
                                        auto next_frame_index = (animation.m_current_frame_index + 1) % current_animation_frame_count;

                                        while (delta_time_between_frams > active_animation_frames->v.frames[next_frame_index].m_frame_time / frame_animation.speed)
                                        {
                                            if (animation.m_loop == false && next_frame_index == current_animation_frame_count - 1)
                                                break;

                                            // 在此应用跳过帧的deltaframe数据
                                            update_and_apply_component_frame_data(e, active_animation_frames->v.frames[next_frame_index]);

                                            delta_time_between_frams -= active_animation_frames->v.frames[next_frame_index].m_frame_time / frame_animation.speed;
                                            next_frame_index = (next_frame_index + 1) % current_animation_frame_count;
                                        }

                                        animation.m_current_frame_index = next_frame_index;
                                        animation.m_next_update_time =
                                            _fixed_time
                                            + active_animation_frames->v.frames[animation.m_current_frame_index].m_frame_time / frame_animation.speed
                                            - delta_time_between_frams
                                            + math::random(-frame_animation.jitter, frame_animation.jitter) / frame_animation.speed;
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
                                                jeecs::debug::logerr("Bad animation data type(%d) when trying set data of uniform variable '%s', please check.",
                                                    (int)udata.m_uniform_value.m_type,
                                                    udata.m_uniform_name.c_str());
                                                break;
                                            }
                                        }
                                    }

                                    if (finish_animation)
                                    {
                                        // 终止动画
                                        animation.set_action("");
                                    }
                                    animation.m_last_speed = frame_animation.speed;
                                }
                            }
                        }
                        else
                        {
                            // 如果没有找到对应的动画，那么终止动画
                            animation.set_action("");
                        }

                        // 这个注释写在这里单纯是因为花括号写得太难看，稍微避免出现一个大于号
                    }

                    // End
                });
        }
    };
}
