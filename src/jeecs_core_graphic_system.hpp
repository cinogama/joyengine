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

const char* shader_light2d_path = "je/shader/light2d.wo";
const char* shader_light2d_src = R"(
// JoyEngineECS RScene shader for light2d-system
// This script only used for forward light2d pipeline.

import je::shader;

let JE_LIGHT2D_DEFER_0 = 4;

let __linear_clamp = sampler2d::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);

public let je_light2d_resolutin = uniform("JOYENGINE_LIGHT2D_RESOLUTION", float2::one);
public let je_light2d_decay = uniform("JOYENGINE_LIGHT2D_DECAY", float::one);

public let je_light2d_defer_albedo = uniform_texture:<texture2d>("JOYENGINE_LIGHT2D_Albedo", __linear_clamp, JE_LIGHT2D_DEFER_0 + 0);
public let je_light2d_defer_self_luminescence = uniform_texture:<texture2d>("JOYENGINE_LIGHT2D_SelfLuminescence", __linear_clamp, JE_LIGHT2D_DEFER_0 + 1);
public let je_light2d_defer_vspace_position = uniform_texture:<texture2d>("JOYENGINE_LIGHT2D_VSpacePosition", __linear_clamp, JE_LIGHT2D_DEFER_0 + 2);
public let je_light2d_defer_vspace_normalize = uniform_texture:<texture2d>("JOYENGINE_LIGHT2D_VSpaceNormalize", __linear_clamp, JE_LIGHT2D_DEFER_0 + 3);
public let je_light2d_defer_shadow = uniform_texture:<texture2d>("JOYENGINE_LIGHT2D_Shadow", __linear_clamp, JE_LIGHT2D_DEFER_0 + 4);
)";

const char* shader_pbr_path = "je/shader/pbr.wo";
const char* shader_pbr_src = R"(
// JoyEngineECS RScene shader tools.
// This script only used for defer light-pbr.

import je::shader;

public let PI = float::new(3.1415926535897932384626);

SHADER_FUNCTION!
public func DistributionGGX(N: float3, H: float3, roughness: float)
{
    let a = roughness * roughness;
    let a2 = a * a;
    let NdotH = max(dot(N, H), float::zero);
    let NdotH2 = NdotH * NdotH;
    
    let nom = a2;
    let denom = NdotH2 * (a2 - float::one) + float::one;
    let pidenom2 = PI * denom * denom;

    return nom / pidenom2;
}

SHADER_FUNCTION!
public func GeometrySchlickGGX(NdotV: float, roughness: float)
{
    let r = roughness + float::one;
    let k = r * r / float::new(8.);
    
    let nom = NdotV;
    let denom = NdotV * (float::one - k) + k;

    return nom / denom;
}

SHADER_FUNCTION!
public func GeometrySmith(N: float3, V: float3, L: float3, roughness: float)
{
    let NdotV = max(dot(N, V), float::zero);
    let NdotL = max(dot(N, L), float::zero);

    let ggx1 = GeometrySchlickGGX(NdotL, roughness);
    let ggx2 = GeometrySchlickGGX(NdotV, roughness);

    return ggx1 * ggx2;
}

SHADER_FUNCTION!
public func FresnelSchlick(cosTheta: float, F0: float3)
{
    return F0 + (float3::one - F0) * pow(float::one - cosTheta, float::new(5.));
}
)";

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
            default_shape_quad =
                graphic::vertex::create(jegl_vertex::TRIANGLESTRIP,
                    {
                        -0.5f, 0.5f, 0.0f,      0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
                        -0.5f, -0.5f, 0.0f,     0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
                        0.5f, 0.5f, 0.0f,       1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
                        0.5f, -0.5f, 0.0f,      1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
                    },
                    { 3, 2, 3 });

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
    where vertex_pos = float4::create(v.vertex, 1.);;

public let frag = 
\_: v2f = fout{ color = float4::create(t, 0., t, 1.) }
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

        using Translation = Transform::Translation;
        using Rendqueue = Renderer::Rendqueue;

        using Clip = Camera::Clip;
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

            bool operator < (const camera_arch& another) const noexcept
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
            const UserInterface::Distortion* ui_distortion;

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

        math::vec3 get_entity_size(const Transform::Translation& trans, const Renderer::Shape& shape)
        {
            math::vec3 size = trans.local_scale;

            const auto& light_shape = shape.vertex != nullptr
                ? shape.vertex
                : m_default_resources.default_shape_quad;

            assert(light_shape->resouce() != nullptr);

            const auto* const raw_vertex_data =
                light_shape->resouce()->m_raw_vertex_data;
            if (raw_vertex_data != nullptr)
            {
                size.x *= raw_vertex_data->m_size_x;
                size.y *= raw_vertex_data->m_size_y;
                size.z *= raw_vertex_data->m_size_z;
            }

            return size;
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

        void CommitUpdate()
        {
            auto WINDOWS_SIZE = jeecs::input::windowsize();
            WINDOWS_WIDTH = (size_t)WINDOWS_SIZE.x;
            WINDOWS_HEIGHT = (size_t)WINDOWS_SIZE.y;

            m_camera_list.clear();
            m_renderer_list.clear();

            this->branch_allocate_begin();

            std::unordered_map<typing::uid_t, UserInterface::Origin*> parent_origin_list;

            auto& selector = select_begin();

            selector.anyof<OrthoProjection, PerspectiveProjection>();
            selector.exec(&UserInterfaceGraphicPipelineSystem::PrepareCameras);

            selector.exec([&](Transform::Anchor& anchor, UserInterface::Origin& origin)
                {
                    parent_origin_list[anchor.uid] = &origin;
                }
            );

            selector.exec([this](Projection& projection, Rendqueue* rendqueue, Viewport* cameraviewport, RendToFramebuffer* rendbuf, Camera::Clear* clear)
                {
                    auto* branch = this->allocate_branch(rendqueue == nullptr ? 0 : rendqueue->rend_queue);
                    m_camera_list.emplace_back(
                        camera_arch{
                            branch, rendqueue, &projection, cameraviewport, rendbuf, nullptr, clear
                        }
                    );
                });

            selector.exec([this, &parent_origin_list](
                Transform::LocalToParent* l2p,
                UserInterface::Origin& origin,
                UserInterface::Absolute* absolute,
                UserInterface::Relatively* relatively)
                {
                    UserInterface::Origin* parent_origin = nullptr;
                    if (l2p != nullptr)
                    {
                        auto fnd = parent_origin_list.find(l2p->parent_uid);
                        if (fnd != parent_origin_list.end())
                            parent_origin = fnd->second;
                    }

                    if (parent_origin != nullptr)
                    {
                        origin.global_offset = parent_origin->global_offset;
                        origin.global_location = parent_origin->global_location;
                        origin.keep_vertical_ratio = parent_origin->keep_vertical_ratio;
                    }
                    else
                    {
                        origin.global_offset = {};
                        origin.global_location = {};
                    }

                    if (absolute != nullptr)
                    {
                        origin.global_offset += absolute->offset;
                        origin.size = absolute->size;
                    }
                    else
                        origin.size = {};

                    if (relatively != nullptr)
                    {
                        origin.global_location += relatively->location;
                        origin.scale = relatively->scale;
                        origin.keep_vertical_ratio = relatively->use_vertical_ratio;
                    }
                    else
                        origin.scale = {};
                }
            );

            selector.anyof<UserInterface::Absolute, UserInterface::Relatively>();
            selector.except<Light2D::Point, Light2D::Parallel>();
            selector.exec(
                [this, &parent_origin_list](
                    Shaders& shads,
                    Textures* texs,
                    Shape& shape,
                    Rendqueue* rendqueue,
                    UserInterface::Origin& origin,
                    UserInterface::Distortion* distortion,
                    Renderer::Color* color)
                {
                    m_renderer_list.emplace_back(
                        renderer_arch{
                            color, rendqueue, nullptr, &shape, &shads, texs, &origin, distortion
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

            float MAT4_UI_MODULE[4][4];

            for (auto& current_camera : m_camera_list)
            {
                assert(current_camera.projection->default_uniform_buffer != nullptr);

                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_v_float4x4),
                    sizeof(float) * 16,
                    MAT4_UI_UNIT);
                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_p_float4x4),
                    sizeof(float) * 16,
                    MAT4_UI_UNIT);
                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_vp_float4x4),
                    sizeof(float) * 16,
                    MAT4_UI_UNIT);
                current_camera.projection->default_uniform_buffer->update_buffer(
                    offsetof(graphic::BasePipelineInterface::default_uniform_buffer_data_t, m_time),
                    sizeof(float) * 4,
                    &shader_time);

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

                    auto uilayout = rendentity.ui_origin->get_layout((float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT);
                    math::vec2 uioffset = math::vec2(uilayout.x, uilayout.y), uisize = math::vec2(uilayout.z, uilayout.w);
                    uioffset.x -= (float)RENDAIMBUFFER_WIDTH / 2.0f;
                    uioffset.y -= (float)RENDAIMBUFFER_HEIGHT / 2.0f;

                    // TODO: 这里俩矩阵实际上可以优化，但是UI实际上也没有多少，暂时直接矩阵乘法也无所谓
                    // NOTE: 这里的大小和偏移大小乘二是因为一致空间是 -1 到 1，天然有一个1/2的压缩，为了保证单位正确，这里乘二
                    const float MAT4_UI_SIZE[4][4] = {
                        { uisize.x * 2.0f / (float)RENDAIMBUFFER_WIDTH , 0.0f, 0.0f, 0.0f },
                        { 0.0f, uisize.y * 2.0f / (float)RENDAIMBUFFER_HEIGHT, 0.0f, 0.0f },
                        { 0.0f, 0.0f, 1.0f, 0.0f },
                        { 0.0f, 0.0f, 0.0f, 1.0f },
                    };
                    const float MAT4_UI_OFFSET[4][4] = {
                       { 1.0f , 0.0f, 0.0f, 0.0f },
                       { 0.0f, 1.0f, 0.0f, 0.0f },
                       { 0.0f, 0.0f, 1.0f, 0.0f },
                       { uioffset.x * 2.0f / (float)RENDAIMBUFFER_WIDTH,
                         uioffset.y * 2.0f / (float)RENDAIMBUFFER_HEIGHT, 0.0f, 1.0f },
                    };

                    float MAT4_UI_ROTATED_SIZE[4][4];

                    float MAT4_UI_ROTATION[4][4] = {
                      { 1.0f, 0.0f, 0.0f, 0.0f },
                      { 0.0f, 1.0f, 0.0f, 0.0f },
                      { 0.0f, 0.0f, 1.0f, 0.0f },
                      { 0.0f, 0.0f, 0.0f, 1.0f },
                    };
                    if (rendentity.ui_distortion != nullptr)
                    {
                        math::quat q(0.0f, 0.0f, rendentity.ui_distortion->angle);
                        q.create_matrix(MAT4_UI_ROTATION);
                    }
                    math::mat4xmat4(MAT4_UI_ROTATED_SIZE, MAT4_UI_SIZE, MAT4_UI_ROTATION);
                    math::mat4xmat4(MAT4_UI_MODULE, MAT4_UI_OFFSET, MAT4_UI_ROTATED_SIZE);

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

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, m, float4x4, MAT4_UI_MODULE);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mvp, float4x4, MAT4_UI_MODULE);
                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, mv, float4x4, MAT4_UI_MODULE);

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

        void CommitUpdate()
        {
            auto WINDOWS_SIZE = jeecs::input::windowsize();
            WINDOWS_WIDTH = (size_t)WINDOWS_SIZE.x;
            WINDOWS_HEIGHT = (size_t)WINDOWS_SIZE.y;

            m_camera_list.clear();
            m_renderer_list.clear();

            this->branch_allocate_begin();

            auto& selector = select_begin();

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
                    m_camera_list.emplace_back(
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
                    // TODO: Need Impl AnyOf
                        // RendOb will be input to a chain and used for swap
                    m_renderer_list.emplace_back(
                        renderer_arch{
                            color, rendqueue, &trans, &shape, &shads, texs
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
                    float MAT4_MODEL[4][4];
                    float MAT4_LOCAL_SCALE[4][4] = {
                        {rendentity.translation->local_scale.x,0.0f,0.0f,0.0f},
                        {0.0f,rendentity.translation->local_scale.y,0.0f,0.0f},
                        {0.0f,0.0f,rendentity.translation->local_scale.z,0.0f},
                        {0.0f,0.0f,0.0f,1.0f} };
                    math::mat4xmat4(MAT4_MODEL, rendentity.translation->object2world, MAT4_LOCAL_SCALE);
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

            DeferLight2DResource()
            {
                using namespace jeecs::graphic;

                _no_shadow = texture::create(1, 1, jegl_texture::format::RGBA);
                _no_shadow->pix(0, 0).set(math::vec4(0.f, 0.f, 0.f, 0.f));

                _screen_vertex = vertex::create(jegl_vertex::type::TRIANGLESTRIP,
                    {
                        -1.f, 1.f, 0.f,     0.f, 1.f,
                        -1.f, -1.f, 0.f,    0.f, 0.f,
                        1.f, 1.f, 0.f,      1.f, 1.f,
                        1.f, -1.f, 0.f,     1.f, 0.f,
                    },
                    { 3, 2 });

                _sprite_shadow_vertex = vertex::create(jegl_vertex::TRIANGLESTRIP,
                    {
                        -0.5f, -0.5f, 0.0f,     0.0f, 1.0f,  1.0f,
                        -0.5f, -0.5f, 0.0f,     0.0f, 0.0f,  0.0f,
                        0.5f, -0.5f, 0.0f,      1.0f, 1.0f,  1.0f,
                        0.5f, -0.5f, 0.0f,      1.0f, 0.0f,  0.0f,
                    },
                    { 3, 2, 1 });

                // 用于消除阴影对象本身的阴影
                _defer_light2d_shadow_sub_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_sub.shader", R"(
import je::shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

VAO_STRUCT! vin 
{
    vertex: float3,
    uv: float2,
};

using v2f = struct{
    pos: float4,
    uv: float2,
};

using fout = struct{
    shadow_factor: float4,
};

public func vert(v: vin)
{
    return v2f{
        pos = je_mvp * float4::create(v.vertex, 1.),
        uv = uvtrans(v.uv, je_tiling, je_offset),
    };
}
public func frag(vf: v2f)
{
    let nearest_clamp = sampler2d::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
    let Main = uniform_texture:<texture2d>("Main", nearest_clamp, 0);
    let final_shadow = alphatest(float4::create(je_color->xyz, texture(Main, vf.uv)->w));

    return fout{
        shadow_factor = float4::create(final_shadow->x, final_shadow->x, final_shadow->x, float::one)
    };
}
)") };

                // 用于产生点光源的形状阴影
                _defer_light2d_shadow_shape_point_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_point_shape.shader", R"(
import je::shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ZERO);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

VAO_STRUCT! vin 
{
    vertex: float3,
    uv: float2,
};

using v2f = struct{
    pos: float4,
    uv: float2,
};

using fout = struct{
    shadow_factor: float4,
};

public func vert(v: vin)
{
    let light2d_vpos = je_v * float4::create(je_color->xyz, 1.);
    let shadow_scale_factor = je_color->w;

    let vpos = je_mv * float4::create(v.vertex, 1.);
    let shadow_vpos = normalize((vpos->xyz / vpos->w) - (light2d_vpos->xyz / light2d_vpos->w)) * shadow_scale_factor;
    
    return v2f{
        pos = je_p * float4::create((vpos->xyz / vpos->w) + shadow_vpos, 1.),
        uv = uvtrans(v.uv, je_tiling, je_offset),
    };
}
public func frag(vf: v2f)
{
    let linear_clamp = sampler2d::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);
    let Main = uniform_texture:<texture2d>("Main", linear_clamp, 0);
    let final_shadow = alphatest(
        float4::create(
            je_local_scale,     // NOTE: je_local_scale->x is shadow factor here.
            texture(Main, vf.uv)->w));

    return fout{
        shadow_factor = float4::create(float3::one, final_shadow->x)
    };
}
)") };

                // 用于产生平行光源的形状阴影
                _defer_light2d_shadow_shape_parallel_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_parallel_shape.shader", R"(
import je::shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ZERO);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

VAO_STRUCT! vin 
{
    vertex: float3,
    uv: float2,
};

using v2f = struct{
    pos: float4,
    uv: float2,
};

using fout = struct{
    shadow_factor: float4,
};

public func vert(v: vin)
{
    let light2d_vdir = (je_v * float4::create(je_color->xyz, 1.))->xyz - movement(je_v);
    let shadow_scale_factor = je_color->w;

    let vpos = je_mv * float4::create(v.vertex, 1.);
    let shadow_vpos = normalize(light2d_vdir) * shadow_scale_factor;
    
    return v2f{
        pos = je_p * float4::create((vpos->xyz / vpos->w) + shadow_vpos, 1.),
        uv = uvtrans(v.uv, je_tiling, je_offset),
    };
}
public func frag(vf: v2f)
{
    let linear_clamp = sampler2d::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);
    let Main = uniform_texture:<texture2d>("Main", linear_clamp, 0);
    let final_shadow = alphatest(
        float4::create(
            je_local_scale,     // NOTE: je_local_scale->x is shadow factor here.
            texture(Main, vf.uv)->w));

    return fout{
        shadow_factor = float4::create(float3::one, final_shadow->x)
    };
}
)") };

                // 用于产生点光源的精灵阴影
                _defer_light2d_shadow_sprite_point_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_point_sprite.shader", R"(
import je::shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ZERO);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

VAO_STRUCT! vin 
{
    vertex: float3,
    uv: float2,
    factor: float,
};

using v2f = struct{
    pos: float4,
    uv: float2,
};

using fout = struct{
    shadow_factor: float4,
};

public func vert(v: vin)
{
    let light2d_vpos = je_v * float4::create(je_color->xyz, 1.);
    let shadow_scale_factor = je_color->w;

    let vpos = je_mv * float4::create(v.vertex, 1.);
    let centerpos = je_mv * float4::create(0., 0., 0., 1.);
    let shadow_vpos = normalize(
        (centerpos->xyz / centerpos->w) - (light2d_vpos->xyz / light2d_vpos->w)
    ) * shadow_scale_factor;
    
    return v2f{
        pos = je_p * float4::create((vpos->xyz / vpos->w) + shadow_vpos * v.factor, 1.),
        uv = uvtrans(v.uv, je_tiling, je_offset),
    };
}
public func frag(vf: v2f)
{
    let linear_clamp = sampler2d::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);
    let Main = uniform_texture:<texture2d>("Main", linear_clamp, 0);
    let final_shadow = alphatest(
        float4::create(
            je_local_scale,     // NOTE: je_local_scale->x is shadow factor here.
            texture(Main, vf.uv)->w));

    return fout{
        shadow_factor = float4::create(float3::one, final_shadow->x)
    };
}
)") };

                // 用于产生平行光源的精灵阴影
                _defer_light2d_shadow_sprite_parallel_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_parallel_sprite.shader", R"(
import je::shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ZERO);
// MUST CULL NONE TO MAKE SURE IF SCALE.X IS NEG.
CULL    (NONE);

VAO_STRUCT! vin 
{
    vertex: float3,
    uv: float2,
    factor: float,
};

using v2f = struct{
    pos: float4,
    uv: float2,
};

using fout = struct{
    shadow_factor: float4,
};

public func vert(v: vin)
{
    let light2d_vdir = (je_v * float4::create(je_color->xyz, 1.))->xyz - movement(je_v);
    let shadow_scale_factor = je_color->w;

    let vpos = je_mv * float4::create(v.vertex, 1.);
    let shadow_vpos = normalize(light2d_vdir) * shadow_scale_factor;
    
    return v2f{
        pos = je_p * float4::create((vpos->xyz / vpos->w) + shadow_vpos * v.factor, 1.),
        uv = uvtrans(v.uv, je_tiling, je_offset),
    };
}
public func frag(vf: v2f)
{
    let linear_clamp = sampler2d::create(LINEAR, LINEAR, LINEAR, CLAMP, CLAMP);
    let Main = uniform_texture:<texture2d>("Main", linear_clamp, 0);
    let final_shadow = alphatest(
        float4::create(
            je_local_scale,     // NOTE: je_local_scale->x is shadow factor here.
            texture(Main, vf.uv)->w));

    return fout{
        shadow_factor = float4::create(float3::one, final_shadow->x)
    };
}
)") };

                // 用于产生点光源的范围阴影
                _defer_light2d_shadow_point_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_point.shader", R"(
import je::shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ZERO);
CULL    (BACK);

VAO_STRUCT! vin
{
    vertex: float3,
    factor: float,
};

using v2f = struct{
    pos: float4,
};

using fout = struct{
    shadow_factor: float4,
};

public func vert(v: vin)
{
    // ATTENTION: We will using je_color: float4 to pass lwpos.
    let light_vpos = je_v * je_color;
    let vpos = je_mv * float4::create(v.vertex, 1.);

    let shadow_vdir = float3::create(normalize(vpos->xy - light_vpos->xy), 0.) * 2000. * v.factor;
    
    return v2f{pos = je_p * float4::create(vpos->xyz + shadow_vdir, 1.)};   
}

public func frag(_: v2f)
{
    // NOTE: je_local_scale->x is shadow factor here.
    return fout{
        shadow_factor = float4::create(float3::one, je_local_scale->x)
    };
}
)") };

                // 用于产生平行光源的范围阴影
                _defer_light2d_shadow_parallel_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_parallel.shader", R"(
import je::shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ZERO);
CULL    (BACK);

VAO_STRUCT! vin
{
    vertex: float3,
    factor: float,
};

using v2f = struct{
    pos: float4,
};

using fout = struct{
    shadow_factor: float4,
};

public func vert(v: vin)
{
    // ATTENTION: We will using je_color: float4 to pass lwpos.
    let light_vdir = (je_v * je_color)->xyz - movement(je_v);
    let vpos = je_mv * float4::create(v.vertex, 1.);

    let shadow_vdir = float3::create(normalize(light_vdir->xy), 0.) * 2000. * v.factor;
    
    return v2f{pos = je_p * float4::create(vpos->xyz + shadow_vdir, 1.)};   
}

public func frag(_: v2f)
{
    // NOTE: je_local_scale->x is shadow factor here.
    return fout{
        shadow_factor = float4::create(float3::one, je_local_scale->x)
    };
}
)") };

                // 用于产生点光源的范围阴影（光在物体后） 逆序
                _defer_light2d_shadow_point_reverse_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_reverse_point.shader", R"(
import je::shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ZERO);
CULL    (FRONT);

VAO_STRUCT! vin
{
    vertex: float3,
    factor: float,
};

using v2f = struct{
    pos: float4,
};

using fout = struct{
    shadow_factor: float4,
};

public func vert(v: vin)
{
    // ATTENTION: We will using je_color: float4 to pass lwpos.
    let light_vpos = je_v * je_color;
    let vpos = je_mv * float4::create(v.vertex, 1.);

    let shadow_vdir = float3::create(normalize(vpos->xy - light_vpos->xy), 0.) * 2000. * v.factor;
    
    return v2f{pos = je_p * float4::create(vpos->xyz + shadow_vdir, 1.)};   
}

public func frag(_: v2f)
{
    // NOTE: je_local_scale->x is shadow factor here.
    return fout{
        shadow_factor = float4::create(float3::one, je_local_scale->x)
    };
}
)") };

                // 用于产生平行光源的范围阴影（光在物体后）逆序
                _defer_light2d_shadow_parallel_reverse_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_reverse_parallel.shader", R"(
import je::shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ZERO);
CULL    (FRONT);

VAO_STRUCT! vin
{
    vertex: float3,
    factor: float,
};

using v2f = struct{
    pos: float4,
};

using fout = struct{
    shadow_factor: float4,
};

public func vert(v: vin)
{
    // ATTENTION: We will using je_color: float4 to pass lwpos.
    let light_vdir = (je_v * je_color)->xyz - movement(je_v);
    let vpos = je_mv * float4::create(v.vertex, 1.);

    let shadow_vdir = float3::create(normalize(light_vdir->xy), 0.) * 2000. * v.factor;
    
    return v2f{pos = je_p * float4::create(vpos->xyz + shadow_vdir, 1.)};   
}

public func frag(_: v2f)
{
    // NOTE: je_local_scale->x is shadow factor here.
    return fout{
        shadow_factor = float4::create(float3::one, je_local_scale->x)
    };
}
)") };
            }
        };

        DeferLight2DResource m_defer_light2d_host;

        using Translation = Transform::Translation;

        using Rendqueue = Renderer::Rendqueue;

        using Clip = Camera::Clip;
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

            bool operator < (const l2dcamera_arch& another) const noexcept
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
            const Light2D::SpriteShadow* spriteshadow;
            const Light2D::SelfShadow* selfshadow;

            const Textures* textures;
            const Shape* shape;
        };

        std::vector<l2dcamera_arch> m_2dcamera_list;
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

        void CommitUpdate()
        {
            auto WINDOWS_SIZE = jeecs::input::windowsize();
            WINDOWS_WIDTH = (size_t)WINDOWS_SIZE.x;
            WINDOWS_HEIGHT = (size_t)WINDOWS_SIZE.y;

            m_2dlight_list.clear();
            m_2dblock_z_list.clear();
            m_2dcamera_list.clear();
            m_renderer_list.clear();

            this->branch_allocate_begin();

            auto& selector = select_begin();

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
                    m_2dcamera_list.emplace_back(
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
                        graphic::framebuffer* rend_aim_buffer = (rendbuf != nullptr && rendbuf->framebuffer != nullptr)
                            ? rendbuf->framebuffer.get()
                            : nullptr;

                        size_t
                            RENDAIMBUFFER_WIDTH =
                            (size_t)llround(
                                (cameraviewport ? cameraviewport->viewport.z : 1.0f) *
                                (rend_aim_buffer ? rend_aim_buffer->width() : WINDOWS_WIDTH)) *
                            std::max(0.001f, std::min(light2dpostpass->ratio, 1.0f)),
                            RENDAIMBUFFER_HEIGHT =
                            (size_t)llround(
                                (cameraviewport ? cameraviewport->viewport.w : 1.0f) *
                                (rend_aim_buffer ? rend_aim_buffer->height() : WINDOWS_HEIGHT)) *
                            std::max(0.001f, std::min(light2dpostpass->ratio, 1.0f));

                        bool need_update = light2dpostpass->post_rend_target == nullptr
                            || light2dpostpass->post_rend_target->width() != RENDAIMBUFFER_WIDTH
                            || light2dpostpass->post_rend_target->height() != RENDAIMBUFFER_HEIGHT;
                        if (need_update && RENDAIMBUFFER_WIDTH > 0 && RENDAIMBUFFER_HEIGHT > 0)
                        {
                            light2dpostpass->post_rend_target
                                = jeecs::graphic::framebuffer::create(RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT,
                                    {
                                        // 漫反射颜色
                                        jegl_texture::format::RGBA,
                                        // 自发光颜色，用于法线反射或者发光物体的颜色参数，最终混合shader会将此参数用于光照计算
                                        jegl_texture::format(jegl_texture::format::RGBA | jegl_texture::format::FLOAT16),
                                        // 视空间坐标(RGB) Alpha通道暂时留空
                                        jegl_texture::format(jegl_texture::format::RGBA | jegl_texture::format::FLOAT16),
                                        // 法线空间颜色
                                        jegl_texture::format(jegl_texture::format::RGBA | jegl_texture::format::FLOAT16),
                                        // 深度缓冲区
                                        jegl_texture::format::DEPTH,
                                    });
                            light2dpostpass->post_light_target
                                = jeecs::graphic::framebuffer::create(RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT,
                                    {
                                        // 光渲染结果
                                        (jegl_texture::format)(jegl_texture::format::RGBA | jegl_texture::format::FLOAT16),
                                    });
                        }
                    }
                });

            selector.except<Light2D::Point, Light2D::Parallel, UserInterface::Origin>();
            selector.exec([this](Translation& trans, Shaders& shads, Textures* texs, Shape& shape, Rendqueue* rendqueue, Renderer::Color* color)
                {
                    // RendOb will be input to a chain and used for swap
                    m_renderer_list.emplace_back(
                        renderer_arch{
                            color, rendqueue, &trans, &shape, &shads, texs
                        });
                });

            selector.anyof<Light2D::Point, Light2D::Parallel>();
            selector.exec([this](Translation& trans,
                Light2D::TopDown* topdown,
                Light2D::Point* point,
                Light2D::Parallel* parallel,
                Light2D::Gain* gain,
                Light2D::ShadowBuffer* shadowbuffer,
                Renderer::Color* color,
                Shape& shape,
                Shaders& shads,
                Textures* texs)
                {
                    m_2dlight_list.emplace_back(
                        light2d_arch{
                            &trans, topdown, point, parallel, gain, shadowbuffer,
                            color, &shape, &shads, texs,
                        }
                    );
                    if (shadowbuffer != nullptr)
                    {
                        bool generate_new_framebuffer =
                            shadowbuffer->buffer == nullptr
                            || shadowbuffer->buffer->width() != shadowbuffer->resolution_width
                            || shadowbuffer->buffer->height() != shadowbuffer->resolution_height;

                        if (generate_new_framebuffer && shadowbuffer->resolution_width > 0 && shadowbuffer->resolution_height > 0)
                        {
                            shadowbuffer->buffer = graphic::framebuffer::create(
                                shadowbuffer->resolution_width, shadowbuffer->resolution_height,
                                {
                                    jegl_texture::format::RGBA,
                                    // Only store shadow value to R-pass
                                }
                            );
                        }
                    }
                });

            selector.anyof<
                Light2D::BlockShadow,
                Light2D::SpriteShadow,
                Light2D::SelfShadow>();

            selector.exec([this](Translation& trans,
                Light2D::BlockShadow* blockshadow,
                Light2D::SpriteShadow* spriteshadow,
                Light2D::SelfShadow* selfshadow,
                Textures* texture,
                Shape* shape)
                {
                    if (blockshadow != nullptr)
                    {
                        if (blockshadow->mesh.m_block_mesh == nullptr)
                        {
                            std::vector<float> _vertex_buffer;
                            if (!blockshadow->mesh.m_block_points.empty())
                            {
                                for (auto& point : blockshadow->mesh.m_block_points)
                                {
                                    _vertex_buffer.insert(_vertex_buffer.end(),
                                        {
                                            point.x, point.y, 0.f, 0.f,
                                            point.x, point.y, 0.f, 1.f,
                                        });
                                }
                                blockshadow->mesh.m_block_mesh = jeecs::graphic::vertex::create(
                                    jegl_vertex::type::TRIANGLESTRIP,
                                    _vertex_buffer, { 3,1 });
                            }
                            else
                                blockshadow->mesh.m_block_mesh = nullptr;
                        }
                    }

                    m_2dblock_z_list.push_back(
                        block2d_arch{
                                &trans,
                                blockshadow,
                                spriteshadow,
                                selfshadow,
                                texture,
                                shape
                        }
                    );
                });

            std::sort(m_2dblock_z_list.begin(), m_2dblock_z_list.end(),
                [](const block2d_arch& a, const block2d_arch& b)
                {
                    return a.translation->world_position.z > b.translation->world_position.z;
                });
            
            m_2dblock_y_list = m_2dblock_z_list;

            std::sort(m_2dblock_y_list.begin(), m_2dblock_y_list.end(),
                [](const block2d_arch& a, const block2d_arch& b)
                {
                    return a.translation->world_position.y < b.translation->world_position.y;
                });

            std::sort(m_2dcamera_list.begin(), m_2dcamera_list.end());
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

            for (auto& current_camera : m_2dcamera_list)
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

                float MAT4_MV[4][4], MAT4_MVP[4][4];

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
                    if (current_camera.light2DPostPass->post_rend_target == nullptr
                        || current_camera.light2DPostPass->post_light_target == nullptr)
                        // Not ready, skip this frame.
                        continue;

                    // Walk throw all light, rend shadows to light's ShadowBuffer.
                    for (auto& lightarch : m_2dlight_list)
                    {
                        const float light_range = 0.5f *
                            get_entity_size(*lightarch.translation, *lightarch.shape).length();

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
                            auto light2d_shadow_aim_buffer = lightarch.shadowbuffer->buffer.get();
                            jegl_rendchain* light2d_shadow_rend_chain = jegl_branch_new_chain(
                                current_camera.branchPipeline,
                                light2d_shadow_aim_buffer->resouce(), 0, 0,
                                light2d_shadow_aim_buffer->width(),
                                light2d_shadow_aim_buffer->height());

                            jegl_rchain_clear_color_buffer(light2d_shadow_rend_chain, nullptr);
                            jegl_rchain_clear_depth_buffer(light2d_shadow_rend_chain);

                            jegl_rchain_bind_uniform_buffer(light2d_shadow_rend_chain,
                                current_camera.projection->default_uniform_buffer->resouce());

                            const auto& normal_shadow_pass =
                                lightarch.parallel != nullptr ?
                                m_defer_light2d_host._defer_light2d_shadow_parallel_pass :
                                m_defer_light2d_host._defer_light2d_shadow_point_pass;
                            const auto& reverse_normal_shadow_pass =
                                lightarch.parallel != nullptr ?
                                m_defer_light2d_host._defer_light2d_shadow_parallel_reverse_pass :
                                m_defer_light2d_host._defer_light2d_shadow_point_reverse_pass;
                            const auto& shape_shadow_pass =
                                lightarch.parallel != nullptr ?
                                m_defer_light2d_host._defer_light2d_shadow_shape_parallel_pass :
                                m_defer_light2d_host._defer_light2d_shadow_shape_point_pass;
                            const auto& sprite_shadow_pass =
                                lightarch.parallel != nullptr ?
                                m_defer_light2d_host._defer_light2d_shadow_sprite_parallel_pass :
                                m_defer_light2d_host._defer_light2d_shadow_sprite_point_pass;

                            const auto& sub_shadow_pass = m_defer_light2d_host._defer_light2d_shadow_sub_pass;
                            const size_t block_entity_count = m_2dblock_y_list.size();

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
                                    get_entity_size(*blockarch.translation, *blockarch.shape).length();

                                const auto l2b_distance = (
                                    blockarch.translation->world_position -
                                    lightarch.translation->world_position
                                    ).length();

                                if (lightarch.parallel != nullptr || l2b_distance <= block_range + light_range)
                                {
                                    block_in_current_layer.push_back(&blockarch);

                                    bool block_is_behind_light = false;

                                    if (blockarch.blockshadow != nullptr && blockarch.blockshadow->factor > 0.f)
                                    {
                                        if (blockarch.blockshadow->mesh.m_block_mesh != nullptr)
                                        {
                                            auto& using_shadow_pass_shader = blockarch.blockshadow->reverse
                                                ? reverse_normal_shadow_pass
                                                : normal_shadow_pass;

                                            auto* rchain_draw_action = jegl_rchain_draw(
                                                light2d_shadow_rend_chain,
                                                using_shadow_pass_shader->resouce(),
                                                blockarch.blockshadow->mesh.m_block_mesh->resouce(),
                                                SIZE_MAX);
                                            auto* builtin_uniform = using_shadow_pass_shader->m_builtin;

                                            float MAT4_MODEL[4][4];
                                            float MAT4_LOCAL_SCALE[4][4] = {
                                                {blockarch.translation->local_scale.x,0.0f,0.0f,0.0f},
                                                {0.0f,blockarch.translation->local_scale.y,0.0f,0.0f},
                                                {0.0f,0.0f,blockarch.translation->local_scale.z,0.0f},
                                                {0.0f,0.0f,0.0f,1.0f} };
                                            math::mat4xmat4(MAT4_MODEL, blockarch.translation->object2world, MAT4_LOCAL_SCALE);
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
                                    if (blockarch.spriteshadow != nullptr && blockarch.spriteshadow->factor > 0.f)
                                    {
                                        auto texture_group = jegl_rchain_allocate_texture_group(light2d_shadow_rend_chain);
                                        if (blockarch.textures != nullptr)
                                        {
                                            jeecs::graphic::texture* main_texture = blockarch.textures->get_texture(0).get();
                                            if (main_texture != nullptr)
                                                jegl_rchain_bind_texture(light2d_shadow_rend_chain, texture_group, 0, main_texture->resouce());
                                            else
                                                jegl_rchain_bind_texture(light2d_shadow_rend_chain, texture_group, 0, m_default_resources.default_texture->resouce());
                                        }

                                        jeecs::graphic::vertex* using_shape = lightarch.topdown == nullptr
                                            ?
                                            (
                                                (blockarch.shape == nullptr || blockarch.shape->vertex == nullptr)
                                                ? m_default_resources.default_shape_quad.get()
                                                : blockarch.shape->vertex.get()
                                                )
                                            :
                                            m_defer_light2d_host._sprite_shadow_vertex.get()
                                            ;

                                        const auto& used_pass = lightarch.topdown == nullptr
                                            ? shape_shadow_pass
                                            : sprite_shadow_pass;

                                        auto* rchain_draw_action = jegl_rchain_draw(
                                            light2d_shadow_rend_chain,
                                            used_pass->resouce(),
                                            using_shape->resouce(),
                                            texture_group);

                                        auto* builtin_uniform = used_pass->m_builtin;

                                        float MAT4_MODEL[4][4];
                                        float MAT4_LOCAL_SCALE[4][4] = {
                                            {blockarch.translation->local_scale.x,0.0f,0.0f,0.0f},
                                            {0.0f,blockarch.translation->local_scale.y,0.0f,0.0f},
                                            {0.0f,0.0f,blockarch.translation->local_scale.z,0.0f},
                                            {0.0f,0.0f,0.0f,1.0f}
                                        };

                                        math::mat4xmat4(MAT4_MODEL, blockarch.translation->object2world, MAT4_LOCAL_SCALE);
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
                                if (next_block2d_iter == block2d_end
                                    || current_layer != (int64_t)((lightarch.topdown == nullptr 
                                        ? next_block2d_iter->translation->world_position.z 
                                        : next_block2d_iter->translation->world_position.y) * 100.f))
                                {
                                    for (auto* block_in_layer : block_in_current_layer)
                                    {
                                        auto texture_group = jegl_rchain_allocate_texture_group(light2d_shadow_rend_chain);
                                        if (block_in_layer->textures != nullptr)
                                        {
                                            jeecs::graphic::texture* main_texture = block_in_layer->textures->get_texture(0).get();
                                            if (main_texture != nullptr)
                                                jegl_rchain_bind_texture(light2d_shadow_rend_chain, texture_group, 0, main_texture->resouce());
                                            else
                                                jegl_rchain_bind_texture(light2d_shadow_rend_chain, texture_group, 0, m_default_resources.default_texture->resouce());
                                        }

                                        jeecs::graphic::vertex* using_shape =
                                            (block_in_layer->shape == nullptr
                                                || block_in_layer->shape->vertex == nullptr)
                                            ? m_default_resources.default_shape_quad.get()
                                            : block_in_layer->shape->vertex.get();

                                        // 如果物体被指定为不需要cover，那么就不绘制
                                        if (block_in_layer->selfshadow != nullptr)
                                        {
                                            auto* rchain_draw_action = jegl_rchain_draw(
                                                light2d_shadow_rend_chain,
                                                sub_shadow_pass->resouce(),
                                                using_shape->resouce(),
                                                texture_group);
                                            auto* builtin_uniform = sub_shadow_pass->m_builtin;

                                            float MAT4_MODEL[4][4];
                                            float MAT4_LOCAL_SCALE[4][4] = {
                                                {block_in_layer->translation->local_scale.x,0.0f,0.0f,0.0f},
                                                {0.0f,block_in_layer->translation->local_scale.y,0.0f,0.0f},
                                                {0.0f,0.0f,block_in_layer->translation->local_scale.z,0.0f},
                                                {0.0f,0.0f,0.0f,1.0f} };
                                            math::mat4xmat4(MAT4_MODEL, block_in_layer->translation->object2world, MAT4_LOCAL_SCALE);
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

                                            if (block_in_layer->selfshadow->auto_uncover &&
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
                                                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, tiling, float2, block_in_layer->textures->tiling.x, block_in_layer->textures->tiling.y);
                                                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, offset, float2, block_in_layer->textures->offset.x, block_in_layer->textures->offset.y);
                                            }
                                        }
                                    }
                                    block_in_current_layer.clear();
                                }
                            }
                        }
                    }

                    auto light2d_rend_aim_buffer =
                        current_camera.light2DPostPass->post_rend_target->resouce();

                    rend_chain = jegl_branch_new_chain(
                        current_camera.branchPipeline,
                        light2d_rend_aim_buffer,
                        0, 0, 0, 0);

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
                    jegl_rchain_clear_depth_buffer(rend_chain);
                }
                else
                {
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

                    // If camera rend to texture, clear the frame buffer (if need)
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
                }

                jegl_rchain_bind_uniform_buffer(rend_chain,
                    current_camera.projection->default_uniform_buffer->resouce());

                auto shadow_pre_bind_texture_group = jegl_rchain_allocate_texture_group(rend_chain);

                jegl_rchain_bind_pre_texture_group(rend_chain, shadow_pre_bind_texture_group);

                constexpr jeecs::math::vec2 default_tiling(1.f, 1.f), default_offset(0.f, 0.f);

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

                    float MAT4_MODEL[4][4];
                    float MAT4_LOCAL_SCALE[4][4] = {
                        {rendentity.translation->local_scale.x,0.0f,0.0f,0.0f},
                        {0.0f,rendentity.translation->local_scale.y,0.0f,0.0f},
                        {0.0f,0.0f,rendentity.translation->local_scale.z,0.0f},
                        {0.0f,0.0f,0.0f,1.0f} };
                    math::mat4xmat4(MAT4_MODEL, rendentity.translation->object2world, MAT4_LOCAL_SCALE);
                    math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                    math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                    auto& drawing_shape =
                        rendentity.shape->vertex != nullptr
                        ? rendentity.shape->vertex
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

                    jegl_rchain_bind_texture(rend_chain, texture_group, 0, m_default_resources.default_texture->resouce());
                    if (rendentity.textures)
                    {
                        _using_tiling = &rendentity.textures->tiling;
                        _using_offset = &rendentity.textures->offset;

                        for (auto& texture : rendentity.textures->textures)
                            jegl_rchain_bind_texture(rend_chain, texture_group, texture.m_pass_id, texture.m_texture->resouce());

                    }
                    for (auto& shader_pass : drawing_shaders)
                    {
                        auto* using_shader = &shader_pass;
                        if (!shader_pass->m_builtin)
                            using_shader = &m_default_resources.default_shader;

                        auto* rchain_draw_action = jegl_rchain_draw(rend_chain, (*using_shader)->resouce(), drawing_shape->resouce(), texture_group);

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

                if (current_camera.light2DPostPass != nullptr
                    && current_camera.shaders != nullptr)
                {
                    // Rend light buffer to target buffer.
                    assert(current_camera.light2DPostPass->post_rend_target != nullptr
                        && current_camera.light2DPostPass->post_light_target != nullptr);

                    // Rend Light result to target buffer.
                    jegl_rendchain* light2d_light_effect_rend_chain = jegl_branch_new_chain(current_camera.branchPipeline,
                        current_camera.light2DPostPass->post_light_target->resouce(),
                        0, 0, 0, 0);

                    jegl_rchain_clear_color_buffer(light2d_light_effect_rend_chain, nullptr);

                    jegl_rchain_bind_uniform_buffer(light2d_light_effect_rend_chain,
                        current_camera.projection->default_uniform_buffer->resouce());

                    auto lightpass_pre_bind_texture_group = jegl_rchain_allocate_texture_group(light2d_light_effect_rend_chain);

                    // Bind attachment
                    // 绑定漫反射颜色通道
                    jegl_rchain_bind_texture(light2d_light_effect_rend_chain, lightpass_pre_bind_texture_group, JE_LIGHT2D_DEFER_0 + 0,
                        current_camera.light2DPostPass->post_rend_target->get_attachment(0)->resouce());
                    // 绑定自发光通道
                    jegl_rchain_bind_texture(light2d_light_effect_rend_chain, lightpass_pre_bind_texture_group, JE_LIGHT2D_DEFER_0 + 1,
                        current_camera.light2DPostPass->post_rend_target->get_attachment(1)->resouce());
                    // 绑定视空间坐标通道
                    jegl_rchain_bind_texture(light2d_light_effect_rend_chain, lightpass_pre_bind_texture_group, JE_LIGHT2D_DEFER_0 + 2,
                        current_camera.light2DPostPass->post_rend_target->get_attachment(2)->resouce());
                    // 绑定视空间法线通道
                    jegl_rchain_bind_texture(light2d_light_effect_rend_chain, lightpass_pre_bind_texture_group, JE_LIGHT2D_DEFER_0 + 3,
                        current_camera.light2DPostPass->post_rend_target->get_attachment(3)->resouce());

                    jegl_rchain_bind_pre_texture_group(light2d_light_effect_rend_chain, lightpass_pre_bind_texture_group);

                    for (auto* light2d_p : _2dlight_after_culling)
                    {
                        auto& light2d = *light2d_p;

                        assert(light2d.translation != nullptr
                            && light2d.color != nullptr
                            && light2d.shaders != nullptr
                            && light2d.shape != nullptr);

                        // 绑定阴影
                        auto texture_group = jegl_rchain_allocate_texture_group(light2d_light_effect_rend_chain);

                        jegl_rchain_bind_texture(light2d_light_effect_rend_chain, texture_group,
                            JE_LIGHT2D_DEFER_0 + 4,
                            light2d.shadowbuffer != nullptr
                            ? light2d.shadowbuffer->buffer->get_attachment(0)->resouce()
                            : m_defer_light2d_host._no_shadow->resouce());

                        // 开始渲染光照！
                        float MAT4_MODEL[4][4];
                        float MAT4_LOCAL_SCALE[4][4] = {
                            {light2d.translation->local_scale.x,0.0f,0.0f,0.0f},
                            {0.0f,light2d.translation->local_scale.y,0.0f,0.0f},
                            {0.0f,0.0f,light2d.translation->local_scale.z,0.0f},
                            {0.0f,0.0f,0.0f,1.0f} };
                        math::mat4xmat4(MAT4_MODEL, light2d.translation->object2world, MAT4_LOCAL_SCALE);
                        math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                        math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                        auto& drawing_shape =
                            light2d.shape->vertex != nullptr
                            ? light2d.shape->vertex
                            : m_default_resources.default_shape_quad;
                        auto& drawing_shaders =
                            light2d.shaders->shaders.empty() == false
                            ? light2d.shaders->shaders
                            : m_default_resources.default_shaders_list;

                        // Bind texture here
                        const jeecs::math::vec2
                            * _using_tiling = &default_tiling,
                            * _using_offset = &default_offset;
                        jegl_rchain_bind_texture(light2d_light_effect_rend_chain, texture_group, 0, m_default_resources.default_texture->resouce());

                        if (light2d.textures != nullptr)
                        {
                            _using_tiling = &light2d.textures->tiling;
                            _using_offset = &light2d.textures->offset;

                            for (auto& texture : light2d.textures->textures)
                                jegl_rchain_bind_texture(light2d_light_effect_rend_chain, texture_group, texture.m_pass_id, texture.m_texture->resouce());
                        }

                        for (auto& shader_pass : drawing_shaders)
                        {
                            auto* using_shader = &shader_pass;
                            if (!shader_pass->m_builtin)
                                using_shader = &m_default_resources.default_shader;

                            auto* rchain_draw_action = jegl_rchain_draw(light2d_light_effect_rend_chain, (*using_shader)->resouce(), drawing_shape->resouce(), texture_group);
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
                            math::vec4 light_color = light2d.color == nullptr ?
                                math::vec4(1.0f, 1.0f, 1.0f, 1.0f) : light2d.color->color;

                            if (light2d.gain != nullptr)
                                light_color.w *= light2d.gain->gain;

                            JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, color, float4,
                                light_color.x,
                                light_color.y,
                                light_color.z,
                                light_color.w);

                            if (light2d.shadowbuffer != nullptr)
                                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_resolution, float2,
                                    (float)light2d.shadowbuffer->resolution_width,
                                    (float)light2d.shadowbuffer->resolution_height);
                            else
                                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_resolution, float2,
                                    (float)1.f,
                                    (float)1.f);

                            if (light2d.point != nullptr)
                                JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_decay, float,
                                    light2d.point->decay);
                        }
                    }

                    // Rend final result color to screen.
                    // Set target buffer.
                    jegl_rendchain* final_target_rend_chain = nullptr;
                    if (current_camera.viewport)
                        final_target_rend_chain = jegl_branch_new_chain(current_camera.branchPipeline,
                            rend_aim_buffer == nullptr ? nullptr : rend_aim_buffer->resouce(),
                            (size_t)(current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH),
                            (size_t)(current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT),
                            (size_t)(current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH),
                            (size_t)(current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT));
                    else
                        final_target_rend_chain = jegl_branch_new_chain(current_camera.branchPipeline,
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
                        jegl_rchain_clear_color_buffer(final_target_rend_chain, clear_buffer_color);
                    }

                    // Clear depth buffer to overwrite pixels.
                    jegl_rchain_clear_depth_buffer(final_target_rend_chain);

                    jegl_rchain_bind_uniform_buffer(final_target_rend_chain,
                        current_camera.projection->default_uniform_buffer->resouce());

                    auto texture_group = jegl_rchain_allocate_texture_group(final_target_rend_chain);

                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, 0,
                        current_camera.light2DPostPass->post_light_target->get_attachment(0)->resouce());

                    // 绑定漫反射颜色通道
                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, JE_LIGHT2D_DEFER_0 + 0,
                        current_camera.light2DPostPass->post_rend_target->get_attachment(0)->resouce());
                    // 绑定自发光通道
                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, JE_LIGHT2D_DEFER_0 + 1,
                        current_camera.light2DPostPass->post_rend_target->get_attachment(1)->resouce());
                    // 绑定视空间坐标通道
                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, JE_LIGHT2D_DEFER_0 + 2,
                        current_camera.light2DPostPass->post_rend_target->get_attachment(2)->resouce());
                    // 绑定视空间法线通道
                    jegl_rchain_bind_texture(final_target_rend_chain, texture_group, JE_LIGHT2D_DEFER_0 + 3,
                        current_camera.light2DPostPass->post_rend_target->get_attachment(3)->resouce());

                    const jeecs::math::vec2
                        * _using_tiling = &default_tiling,
                        * _using_offset = &default_offset;

                    if (current_camera.textures)
                    {
                        _using_tiling = &current_camera.textures->tiling;
                        _using_offset = &current_camera.textures->offset;

                        for (auto& texture : current_camera.textures->textures)
                            jegl_rchain_bind_texture(final_target_rend_chain, texture_group, texture.m_pass_id, texture.m_texture->resouce());
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
                            (*using_shader)->resouce(), m_defer_light2d_host._screen_vertex->resouce(), texture_group);

                        auto* builtin_uniform = (*using_shader)->m_builtin;

                        JE_CHECK_NEED_AND_SET_UNIFORM(rchain_draw_action, builtin_uniform, light2d_resolution, float2,
                            (float)current_camera.light2DPostPass->post_light_target->width(),
                            (float)current_camera.light2DPostPass->post_light_target->height());

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

    struct FrameAnimation2DSystem : public game_system
    {
        double _fixed_time = 0.;

        FrameAnimation2DSystem(game_world w)
            : game_system(w)
        {

        }

        void StateUpdate()
        {
            _fixed_time += deltatime();

            auto& selector = select_begin();

            selector.exec([this](game_entity e, Animation2D::FrameAnimation& frame_animation, Renderer::Shaders* shaders)
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
                                        [](const game_entity& e, jeecs::Animation2D::FrameAnimation::animation_data_set_list::frame_data& frame)
                                        {
                                            for (auto& cdata : frame.m_component_data)
                                            {
                                                if (cdata.m_entity_cache == e)
                                                    continue;

                                                cdata.m_entity_cache = e;

                                                assert(cdata.m_component_type != nullptr && cdata.m_member_info != nullptr);

                                                auto* component_addr = je_ecs_world_entity_get_component(&e, cdata.m_component_type);
                                                if (component_addr == nullptr)
                                                    // 没有这个组件，忽略之
                                                    continue;

                                                auto* member_addr = (void*)(cdata.m_member_info->m_member_offset + (intptr_t)component_addr);

                                                // 在这里做好缓存和检查，不要每次都重新获取组件地址和检查类型
                                                cdata.m_member_addr_cache = member_addr;

                                                switch (cdata.m_member_value.m_type)
                                                {
                                                case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::INT:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<int>())
                                                    {
                                                        jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'int', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::FLOAT:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<float>())
                                                    {
                                                        jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'float', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC2:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec2>())
                                                    {
                                                        jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'vec2', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC3:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec3>())
                                                    {
                                                        jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'vec3', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC4:
                                                    if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec4>())
                                                    {
                                                        jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'vec4', but member is '%s'.",
                                                            cdata.m_component_type->m_typename,
                                                            cdata.m_member_info->m_member_name,
                                                            cdata.m_member_info->m_member_type->m_typename);
                                                        cdata.m_member_addr_cache = nullptr;
                                                    }
                                                    break;
                                                case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::QUAT4:
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
                                                case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::INT:
                                                    if (cdata.m_offset_mode)
                                                        *(int*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.i32;
                                                    else
                                                        *(int*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.i32;
                                                    break;
                                                case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::FLOAT:
                                                    if (cdata.m_offset_mode)
                                                        *(float*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.f32;
                                                    else
                                                        *(float*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.f32;
                                                    break;
                                                case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC2:
                                                    if (cdata.m_offset_mode)
                                                        *(math::vec2*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.v2;
                                                    else
                                                        *(math::vec2*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.v2;
                                                    break;
                                                case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC3:
                                                    if (cdata.m_offset_mode)
                                                        *(math::vec3*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.v3;
                                                    else
                                                        *(math::vec3*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.v3;
                                                    break;
                                                case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC4:
                                                    if (cdata.m_offset_mode)
                                                        *(math::vec4*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.v4;
                                                    else
                                                        *(math::vec4*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.v4;
                                                    break;
                                                case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::QUAT4:
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
                                        auto current_animation_frame_count = active_animation_frames->v.frames.size();

                                        auto delta_time_between_frams = _fixed_time - animation.m_next_update_time;
                                        auto next_frame_index = (animation.m_current_frame_index + 1) % current_animation_frame_count;

                                        while (delta_time_between_frams > active_animation_frames->v.frames[next_frame_index].m_frame_time / frame_animation.speed)
                                        {
                                            if (animation.m_loop == false && next_frame_index == 0)
                                                break;

                                            // 在此应用跳过帧的deltaframe数据
                                            update_and_apply_component_frame_data(e, active_animation_frames->v.frames[next_frame_index]);

                                            delta_time_between_frams -= active_animation_frames->v.frames[next_frame_index].m_frame_time / frame_animation.speed;
                                            next_frame_index = (next_frame_index + 1) % current_animation_frame_count;
                                        }

                                        if (animation.m_loop == false && next_frame_index == 0)
                                        {
                                            // 帧动画播放完毕，最后更新到最后一帧，然后终止动画
                                            finish_animation = true;
                                            next_frame_index = current_animation_frame_count - 1;
                                        }

                                        animation.m_current_frame_index = next_frame_index;
                                        animation.m_next_update_time =
                                            _fixed_time
                                            + active_animation_frames->v.frames[animation.m_current_frame_index].m_frame_time / frame_animation.speed
                                            - delta_time_between_frams
                                            + math::random(-frame_animation.jitter, frame_animation.jitter) / frame_animation.speed;
                                    }

                                    auto& updating_frame = active_animation_frames->v.frames[animation.m_current_frame_index];
                                    update_and_apply_component_frame_data(e, updating_frame);

                                    if (shaders != nullptr)
                                    {
                                        for (auto& udata : updating_frame.m_uniform_data)
                                        {
                                            switch (udata.m_uniform_value.m_type)
                                            {
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::INT:
                                                shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.i32);
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::FLOAT:
                                                shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.f32);
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC2:
                                                shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.v2);
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC3:
                                                shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.v3);
                                                break;
                                            case Animation2D::FrameAnimation::animation_data_set_list::frame_data::data_value::type::VEC4:
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
