#pragma once

// Important:
// Graphic is a job, not a system.

#ifndef JE_IMPL
#   define JE_IMPL
#   define JE_ENABLE_DEBUG_API
#   include "jeecs.hpp"
#endif

#include <queue>
#include <list>

#define JE_MAX_LIGHT2D_COUNT 16
#define JE_SHADOW2D_0 24

#define JE_LIGHT2D_DEFER_0 16

const char* shader_light2d_path = "je/shader/light2d.wo";
const char* shader_light2d_src = R"(
// JoyEngineECS RScene shader for light2d-system
// This script only used for forward light2d pipeline.

import je.shader;

public let MAX_SHADOW_LIGHT_COUNT = 16;
let SHADOW2D_TEX_0 = 24;
let DEFER_TEX_0 = 16;

// define struct for Light
GRAPHIC_STRUCT! Light2D
{
    color:      float4,  // color->xyz is color, color->w is intensity.
    position:   float4,  // position->xyz used for point-light
                         // position->w is range that calced by local-scale & shape size.
    direction:  float4,  // direction->xyz used for parallel-light
                         // direction->w is reserved.
    factors:    float4,  // factors->x & y used for effect position or parallel
                         // factors->z is decay, normally, parallel light's decay should be 0
                         // factors->w is reserved.
};

UNIFORM_BUFFER! JOYENGINE_LIGHT2D = 1
{
    // Append nothing here, add them later.
};

// Create lights for JOYENGINE_LIGHT2D
public let je_light2ds = func(){
    let lights = []mut: vec<Light2D_t>;

    for (let mut i = 0;i < MAX_SHADOW_LIGHT_COUNT; i += 1)
        lights->add(JOYENGINE_LIGHT2D->append_struct_uniform(F"JE_LIGHT2D_{i}", Light2D): gchandle: Light2D_t);

    return lights->toarray;
}();

public let je_shadow2ds = func(){
    let shadows = []mut: vec<texture2d>;

    for (let mut i = 0;i < MAX_SHADOW_LIGHT_COUNT; i += 1)
        shadows->add(uniform_texture:<texture2d>(F"JOYENGINE_SHADOW2D_{i}", SHADOW2D_TEX_0 + i));

    return shadows->toarray;
}();


public let je_shadow2d_resolutin = uniform("JOYENGINE_SHADOW2D_RESOLUTION", float2_one);
public let je_light2d_decay = uniform("JOYENGINE_LIGHT2D_DECAY", float_one);

public let je_light2d_defer_albedo = uniform_texture:<texture2d>("JOYENGINE_LIGHT2D_Albedo", DEFER_TEX_0 + 0);
public let je_light2d_defer_self_luminescence = uniform_texture:<texture2d>("JOYENGINE_LIGHT2D_SelfLuminescence", DEFER_TEX_0 + 1);
public let je_light2d_defer_visual_coord = uniform_texture:<texture2d>("JOYENGINE_LIGHT2D_VisualCoordinates", DEFER_TEX_0 + 2);
public let je_light2d_defer_shadow = uniform_texture:<texture2d>("JOYENGINE_LIGHT2D_Shadow", DEFER_TEX_0 + 3);
)";

const char* shader_pbr_path = "je/shader/pbr.wo";
const char* shader_pbr_src = R"(
// JoyEngineECS RScene shader tools.
// This script only used for defer light-pbr.

import je.shader;

public let PI = float::new(3.1415926535897932384626);

public func DistributionGGX(N: float3, H: float3, roughness: float)
{
    let a = roughness * roughness;
    let a2 = a * a;
    let NdotH = max(dot(N, H), float_zero);
    let NdotH2 = NdotH * NdotH;
    
    let nom = a2;
    let denom = NdotH2 * (a2 - float_one) + float_one;
    let pidenom2 = PI * denom * denom;

    return nom / pidenom2;
}

public func GeometrySchlickGGX(NdotV: float, roughness: float)
{
    let r = roughness + float_one;
    let k = r * r / float::new(8.);
    
    let nom = NdotV;
    let denom = NdotV * (float_one - k) + k;

    return nom / denom;
}

public func GeometrySmith(N: float3, V: float3, L: float3, roughness: float)
{
    let NdotV = max(dot(N, V), float_zero);
    let NdotL = max(dot(N, L), float_zero);

    let ggx1 = GeometrySchlickGGX(NdotL, roughness);
    let ggx2 = GeometrySchlickGGX(NdotV, roughness);

    return ggx1 * ggx2;
}

public func FresnelSchlick(cosTheta: float, F0: float3)
{
    return F0 + (float3_one - F0) * pow(float_one - cosTheta, float::new(5.));
}

public func multi_sampling_for_bias_shadow(shadow: texture2d, reso: float2, uv: float2)
{
    let mut shadow_factor = float_zero;
    let bias = 2.;

    let bias_weight = [
        (0., 0., 1.)
        //(-2., 2., 0.08),    (-1., 2., 0.08),    (0., 2., 0.08),     (1., 2., 0.08),     (2., 2., 0.08),
        //(-2., 1., 0.08),    (-1., 1., 0.08),    (0., 1., 0.16),     (1., 1., 0.16),     (2., 1., 0.08),
        //(-2., 0., 0.08),    (-1., 0., 0.08),    (0., 0., 0.72),     (1., 0., 0.16),     (2., 0., 0.08),
        //(-2., -1., 0.08),   (-1., -1., 0.16),   (0., -1., 0.16),    (1., -1., 0.16),    (2., -1., 0.08),
        //(-2., -2., 0.08),   (-1., -2., 0.08),   (0., -2., 0.08),    (1., -2., 0.08),    (2., -2., 0.08),
    ];

    let reso_inv = float2_one / reso;

    for (let _, (x, y, weight) : bias_weight)
    {
        shadow_factor = shadow_factor + texture(
            shadow, uv + reso_inv * float2::create(x, y) * bias
        )->x * weight;
        
    }
    return clamp(shadow_factor, 0., 1.);
}

)";

namespace jeecs
{
    struct GraphicPipelineBaseSystem : public game_system
    {
        void (*m_frame_update_interface)(GraphicPipelineBaseSystem*);
        int m_priority;

        GraphicPipelineBaseSystem(game_world w, void (*_frame_update_interface)(GraphicPipelineBaseSystem*))
            : game_system(w)
            , m_priority(0)
            , m_frame_update_interface(_frame_update_interface)
        {
            assert(m_frame_update_interface != nullptr);
        }
        void reset_priority(int priority)
        {
            m_priority = priority;
        }
        void update_priority(int priority)
        {
            if (priority > m_priority)
                m_priority = priority;
        }
        void update_frame()
        {
            m_frame_update_interface(this);
        }
    };

    struct GraphicThreadHost
    {
        JECS_DISABLE_MOVE_AND_COPY(GraphicThreadHost);

        jegl_thread* glthread = nullptr;
        jeecs::game_universe universe;
        basic::resource<graphic::vertex> default_shape_quad;
        basic::resource<graphic::shader> default_shader;
        basic::resource<graphic::texture> default_texture;
        jeecs::vector<basic::resource<graphic::shader>> default_shaders_list;

        static double _update_frame_universe_job(void* host)
        {
            auto* graphic_host = (GraphicThreadHost*)host;
            if (!jegl_update(graphic_host->glthread))
            {
                graphic_host->universe.stop();
            }

            return 1.0 / (double)graphic_host->glthread->m_config.m_fps;
        }

        std::mutex m_graphic_pipelines_mx;
        std::vector<GraphicPipelineBaseSystem*> m_graphic_pipelines;

        void register_pipeline(GraphicPipelineBaseSystem* sys)
        {
            std::lock_guard g1(m_graphic_pipelines_mx);
            m_graphic_pipelines.push_back(sys);
        }

        void unregister_pipeline(GraphicPipelineBaseSystem* sys)
        {
            std::lock_guard g1(m_graphic_pipelines_mx);
            auto fnd = std::find(m_graphic_pipelines.begin(), m_graphic_pipelines.end(), sys);
            assert(fnd != m_graphic_pipelines.end());
            m_graphic_pipelines.erase(fnd);
        }

        void _frame_rend_impl()
        {
            // Clear frame buffer
            jegl_clear_framebuffer(nullptr);

            std::lock_guard g1(m_graphic_pipelines_mx);

            std::stable_sort(m_graphic_pipelines.begin(), m_graphic_pipelines.end(), 
                [](GraphicPipelineBaseSystem* a, GraphicPipelineBaseSystem* b)
                {
                    return a->m_priority < b->m_priority;
                });

            for (auto * gpipe : m_graphic_pipelines)
            {
                gpipe->update_frame();
            }
        }

        GraphicThreadHost(jeecs::game_universe _universe)
            : universe(_universe)
        {
            default_shape_quad =
                new graphic::vertex(jegl_vertex::TRIANGLESTRIP,
                    {
                        -0.5f, 0.5f, 0.0f,      0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
                        -0.5f, -0.5f, 0.0f,     0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
                        0.5f, 0.5f, 0.0f,       1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
                        0.5f, -0.5f, 0.0f,      1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
                    },
                    { 3, 2, 3 });

            default_texture = graphic::texture::create(2, 2, jegl_texture::texture_format::RGBA);
            default_texture->pix(0, 0).set({ 1.f, 0.25f, 1.f, 1.f });
            default_texture->pix(1, 1).set({ 1.f, 0.25f, 1.f, 1.f });
            default_texture->pix(0, 1).set({ 0.25f, 0.25f, 0.25f, 1.f });
            default_texture->pix(1, 0).set({ 0.25f, 0.25f, 0.25f, 1.f });
            default_texture->resouce()->m_raw_texture_data->m_sampling = jegl_texture::texture_sampling::NEAREST;

            default_shader = graphic::shader::create("!/builtin/builtin_default.shader", R"(
// Default shader
import je.shader;

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
\f: v2f = fout{ color = float4::create(t, 0., t, 1.) }
    where t = je_time->y();;

)");
            default_shaders_list.push_back(default_shader);

            jegl_interface_config config = {};
            config.m_fps = 60;
            config.m_resolution_x = 640;
            config.m_resolution_y = 480;
            config.m_title = "JoyEngineECS(JoyEngine 4.0)";

            glthread = jegl_start_graphic_thread(
                config,
                jegl_using_opengl3_apis,
                [](void* ptr, jegl_thread* glthread)
                {
                    ((GraphicThreadHost*)ptr)->_frame_rend_impl();
                }, this);

            je_ecs_universe_register_pre_call_once_job(universe.handle(), _update_frame_universe_job, this, nullptr);
        }

        ~GraphicThreadHost()
        {
            assert(this == _m_instance);
            _m_instance = nullptr;
            if (glthread)
                jegl_terminate_graphic_thread(glthread);

            je_ecs_universe_unregister_pre_call_once_job(universe.handle(), _update_frame_universe_job);
        }

        inline static std::atomic<GraphicThreadHost*> _m_instance = nullptr;

        static GraphicThreadHost* get_default_graphic_pipeline_instance(game_universe universe)
        {
            if (nullptr == _m_instance)
            {
                _m_instance = new GraphicThreadHost(universe);
                je_ecs_universe_register_exit_callback(universe.handle(),
                    [](void* instance)
                    {
                        delete (GraphicThreadHost*)instance;
                    },
                    _m_instance);
            }

            assert(_m_instance);
            return _m_instance;
        }       
    };

    template<typename PipelineT>
    struct GraphicPipelineInterfaceSystem : GraphicPipelineBaseSystem
    {
        GraphicThreadHost* _m_pipeline;

        GraphicPipelineInterfaceSystem(game_world w)
            : GraphicPipelineBaseSystem(w, &PipelineT::Frame)
            , _m_pipeline(GraphicThreadHost::get_default_graphic_pipeline_instance(w.get_universe()))
        {
            _m_pipeline->register_pipeline(this);
        }
        ~GraphicPipelineInterfaceSystem()
        {
            _m_pipeline->unregister_pipeline(this);
        }

        inline GraphicThreadHost* host()const noexcept
        {
            return _m_pipeline;
        }
    };

    struct DefaultGraphicPipelineSystem : public GraphicPipelineInterfaceSystem<DefaultGraphicPipelineSystem>
    {
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

        struct camera_arch
        {
            const Rendqueue* rendqueue;
            const Projection* projection;
            const Viewport* viewport;
            const RendToFramebuffer* rendToFramebuffer;

            bool operator < (const camera_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };
        struct renderer_arch
        {
            const Rendqueue* rendqueue;
            const Translation* translation;
            const Shape* shape;
            const Shaders* shaders;
            const Textures* textures;

            bool operator < (const renderer_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };

        std::vector<camera_arch> m_camera_list;
        std::vector<renderer_arch> m_renderer_list;

        size_t WINDOWS_WIDTH = 0;
        size_t WINDOWS_HEIGHT = 0;

        jeecs::basic::resource<jeecs::graphic::uniformbuffer> m_default_uniform_buffer;
        struct default_uniform_buffer_data_t
        {
            jeecs::math::vec4 time;
        };

        DefaultGraphicPipelineSystem(game_world w)
            : GraphicPipelineInterfaceSystem(w)
        {
            m_default_uniform_buffer = new jeecs::graphic::uniformbuffer(0, sizeof(default_uniform_buffer_data_t));
        }

        ~DefaultGraphicPipelineSystem()
        {

        }

        void PrepareCameras(Projection& projection,
            Translation& translation,
            OrthoProjection* ortho,
            PerspectiveProjection* perspec,
            Clip* clip,
            Viewport* viewport,
            RendToFramebuffer* rendbuf)
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

            jegl_resource* rend_aim_buffer = rendbuf && rendbuf->framebuffer ? rendbuf->framebuffer->resouce() : nullptr;

            size_t
                RENDAIMBUFFER_WIDTH =
                (size_t)llround(
                    (viewport ? viewport->viewport.z : 1.0f) *
                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_width : WINDOWS_WIDTH)),
                RENDAIMBUFFER_HEIGHT =
                (size_t)llround(
                    (viewport ? viewport->viewport.w : 1.0f) *
                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_height : WINDOWS_HEIGHT));

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
        }

        void PreUpdate()
        {
            m_camera_list.clear();
            m_renderer_list.clear();

            select_from(get_world())
                .exec(&DefaultGraphicPipelineSystem::PrepareCameras).anyof<OrthoProjection, PerspectiveProjection>()
                .exec(
                    [this](Projection& projection, Rendqueue* rendqueue, Viewport* cameraviewport, RendToFramebuffer* rendbuf)
                    {
                        if (rendqueue != nullptr)
                            update_priority(rendqueue->rend_queue);

                        m_camera_list.emplace_back(
                            camera_arch{
                                rendqueue, &projection, cameraviewport, rendbuf
                            }
                        );
                    })
                .exec(
                    [this](Translation& trans, Shaders* shads, Textures* texs, Shape* shape, Rendqueue* rendqueue)
                    {
                        // TODO: Need Impl AnyOf
                            // RendOb will be input to a chain and used for swap
                        m_renderer_list.emplace_back(
                            renderer_arch{
                                rendqueue, &trans, shape, shads, texs
                            });
                    })
                        .anyof<Shaders, Textures, Shape>()
                        .except<Light2D::Color>()
                        ;
                    std::sort(m_camera_list.begin(), m_camera_list.end());
                    std::sort(m_renderer_list.begin(), m_renderer_list.end());
        }
        static void Frame(GraphicPipelineBaseSystem* pipe)
        {
            static_cast<DefaultGraphicPipelineSystem*>(pipe)->DrawFrame();
        }
        void DrawFrame()
        {
            jegl_get_windows_size(&WINDOWS_WIDTH, &WINDOWS_HEIGHT);

            // TODO: Update shared uniform.
            double current_time = je_clock_time();

            math::vec4 shader_time =
            {
                (float)current_time ,
                (float)abs(2.0 * (current_time * 2.0 - double(int(current_time * 2.0)) - 0.5)) ,
                (float)abs(2.0 * (current_time - double(int(current_time)) - 0.5)),
                (float)abs(2.0 * (current_time / 2.0 - double(int(current_time / 2.0)) - 0.5))
            };

            m_default_uniform_buffer->update_buffer(
                offsetof(default_uniform_buffer_data_t, time),
                sizeof(math::vec4),
                &shader_time);

            jegl_using_resource(m_default_uniform_buffer->resouce());

            for (auto& current_camera : m_camera_list)
            {
                jegl_resource* rend_aim_buffer = nullptr;
                if (current_camera.rendToFramebuffer)
                {
                    if (current_camera.rendToFramebuffer->framebuffer == nullptr)
                        continue;
                    else
                        rend_aim_buffer = current_camera.rendToFramebuffer->framebuffer->resouce();
                }

                size_t
                    RENDAIMBUFFER_WIDTH = rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_width : WINDOWS_WIDTH,
                    RENDAIMBUFFER_HEIGHT = rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_height : WINDOWS_HEIGHT;

                const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
                const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;

                float MAT4_MV[4][4], MAT4_VP[4][4];
                math::mat4xmat4(MAT4_VP, MAT4_PROJECTION, MAT4_VIEW);

                if (current_camera.viewport)
                    jegl_rend_to_framebuffer(rend_aim_buffer,
                        (size_t)(current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH),
                        (size_t)(current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT),
                        (size_t)(current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH),
                        (size_t)(current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT));
                else
                    jegl_rend_to_framebuffer(rend_aim_buffer, 0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                // If camera rend to texture, clear the frame buffer (if need)
                if (rend_aim_buffer)
                    jegl_clear_framebuffer(rend_aim_buffer);


                // Walk through all entities, rend them to target buffer(include L2DCamera/R2Buf/Screen).
                for (auto& rendentity : m_renderer_list)
                {
                    assert(rendentity.translation);

                    const float(&MAT4_MODEL)[4][4] = rendentity.translation->object2world;

                    float MAT4_MVP[4][4];
                    math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                    math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                    auto& drawing_shape =
                        (rendentity.shape && rendentity.shape->vertex)
                        ? rendentity.shape->vertex
                        : host()->default_shape_quad;
                    auto& drawing_shaders =
                        (rendentity.shaders && rendentity.shaders->shaders.size())
                        ? rendentity.shaders->shaders
                        : host()->default_shaders_list;

                    // Bind texture here
                    constexpr jeecs::math::vec2 default_tiling(1.f, 1.f), default_offset(0.f, 0.f);
                    const jeecs::math::vec2
                        * _using_tiling = &default_tiling,
                        * _using_offset = &default_offset;
                    jegl_using_texture(host()->default_texture->resouce(), 0);
                    if (rendentity.textures)
                    {
                        _using_tiling = &rendentity.textures->tiling;
                        _using_offset = &rendentity.textures->offset;

                        for (auto& texture : rendentity.textures->textures)
                            jegl_using_texture(texture.m_texture->resouce(), texture.m_pass_id);
                    }
                    for (auto& shader_pass : drawing_shaders)
                    {
                        auto* using_shader = &shader_pass;
                        if (!shader_pass->m_builtin)
                            using_shader = &host()->default_shader;

                        jegl_using_resource((*using_shader)->resouce());

                        auto* builtin_uniform = (*using_shader)->m_builtin;
#define NEED_AND_SET_UNIFORM(ITEM, TYPE, ...) \
if (builtin_uniform->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
    jegl_uniform_##TYPE(shader_pass->resouce(), builtin_uniform->m_builtin_uniform_##ITEM, __VA_ARGS__)

                        NEED_AND_SET_UNIFORM(m, float4x4, MAT4_MODEL);
                        NEED_AND_SET_UNIFORM(v, float4x4, MAT4_VIEW);
                        NEED_AND_SET_UNIFORM(p, float4x4, MAT4_PROJECTION);

                        NEED_AND_SET_UNIFORM(mv, float4x4, MAT4_MV);
                        NEED_AND_SET_UNIFORM(vp, float4x4, MAT4_VP);
                        NEED_AND_SET_UNIFORM(mvp, float4x4, MAT4_MVP);

                        NEED_AND_SET_UNIFORM(local_scale, float3,
                            rendentity.translation->local_scale.x,
                            rendentity.translation->local_scale.y,
                            rendentity.translation->local_scale.z);

                        NEED_AND_SET_UNIFORM(tiling, float2, _using_tiling->x, _using_tiling->y);
                        NEED_AND_SET_UNIFORM(offset, float2, _using_offset->x, _using_offset->y);

#undef NEED_AND_SET_UNIFORM
                        jegl_draw_vertex(drawing_shape->resouce());
                    }

                }

            }

            // Redirect to screen space for imgui rend.
            jegl_rend_to_framebuffer(nullptr, 0, 0, WINDOWS_WIDTH, WINDOWS_HEIGHT);
        }

    };

    struct DeferLight2DGraphicPipelineSystem : public GraphicPipelineInterfaceSystem<DeferLight2DGraphicPipelineSystem>
    {
        struct DeferLight2DHost
        {
            jegl_thread* _m_belong_context;

            // Used for move rend result to camera's render aim buffer.
            jeecs::basic::resource<jeecs::graphic::texture> _no_shadow;
            jeecs::basic::resource<jeecs::graphic::vertex> _screen_vertex;

            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_point_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_parallel_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_shape_point_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_shape_parallel_pass;

            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sub_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_mix_light_effect_pass;

            DeferLight2DHost(jegl_thread* _ctx)
                : _m_belong_context(_ctx)
            {
                using namespace jeecs::graphic;
                _no_shadow = texture::create(1, 1, jegl_texture::texture_format::RGBA);
                _no_shadow->pix(0, 0).set(math::vec4(0.f, 0.f, 0.f, 0.f));

                _screen_vertex = new vertex(jegl_vertex::vertex_type::TRIANGLESTRIP,
                    {
                        -1.f, 1.f, 0.f,     0.f, 1.f,
                        -1.f, -1.f, 0.f,    0.f, 0.f,
                        1.f, 1.f, 0.f,      1.f, 1.f,
                        1.f, -1.f, 0.f,     1.f, 0.f,
                    },
                    { 3, 2 });

                // 用于消除阴影对象本身的阴影
                _defer_light2d_shadow_sub_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_sub.shader", R"(
import je.shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (BACK);

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
    shadow_factor: float,
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
    let main_texture = uniform_texture:<texture2d>("MainTexture", 0);
    let final_shadow = alphatest(float4::create(je_color->xyz, texture(main_texture, vf.uv)->w));

    return fout{
        shadow_factor = final_shadow->x
    };
}
)") };

                // 用于产生点光源的形状阴影（光在物体前）
                _defer_light2d_shadow_shape_point_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_point_shape.shader", R"(
import je.shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (BACK);

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
    shadow_factor: float,
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
    let main_texture = uniform_texture:<texture2d>("MainTexture", 0);
    let final_shadow = alphatest(
        float4::create(
            je_local_scale,     // NOTE: je_local_scale->x is shadow factor here.
            texture(main_texture, vf.uv)->w));

    return fout{
        shadow_factor = final_shadow->x
    };
}
)") };

                // 用于产生点光源的范围阴影（光在物体后）
                _defer_light2d_shadow_point_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_point.shader", R"(
import je.shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);

VAO_STRUCT! vin
{
    vertex: float3,
    factor: float,
};

using v2f = struct{
    pos: float4,
};

using fout = struct{
    shadow_factor: float,
};

public func vert(v: vin)
{
    // ATTENTION: We will using je_color: float4 to pass lwpos.
    let light_vpos = je_v * je_color;
    let vpos = je_mv * float4::create(v.vertex, 1.);

    let shadow_vdir = float3::create(normalize(vpos->xy - light_vpos->xy), 0.) * 2000. * v.factor;
    
    return v2f{pos = je_p * float4::create(vpos->xyz + shadow_vdir, 1.)};   
}

public func frag(vf: v2f)
{
    // NOTE: je_local_scale->x is shadow factor here.
    return fout{shadow_factor = je_local_scale->x};
}
)") };

                // 用于产生平行光源的形状阴影（光在物体前）
                _defer_light2d_shadow_shape_parallel_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_parallel_shape.shader", R"(
import je.shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (BACK);

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
    shadow_factor: float,
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
    let main_texture = uniform_texture:<texture2d>("MainTexture", 0);
    let final_shadow = alphatest(
        float4::create(
            je_local_scale,     // NOTE: je_local_scale->x is shadow factor here.
            texture(main_texture, vf.uv)->w));

    return fout{
        shadow_factor = final_shadow->x
    };
}
)") };

                // 用于产生平行光源的范围阴影（光在物体后）
                _defer_light2d_shadow_parallel_pass
                    = { shader::create("!/builtin/defer_light2d_shadow_parallel.shader", R"(
import je.shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);

VAO_STRUCT! vin
{
    vertex: float3,
    factor: float,
};

using v2f = struct{
    pos: float4,
};

using fout = struct{
    shadow_factor: float,
};

public func vert(v: vin)
{
    // ATTENTION: We will using je_color: float4 to pass lwpos.
    let light_vdir = (je_v * je_color)->xyz - movement(je_v);
    let vpos = je_mv * float4::create(v.vertex, 1.);

    let shadow_vdir = float3::create(normalize(light_vdir->xy), 0.) * 2000. * v.factor;
    
    return v2f{pos = je_p * float4::create(vpos->xyz + shadow_vdir, 1.)};   
}

public func frag(vf: v2f)
{
    // NOTE: je_local_scale->x is shadow factor here.
    return fout{shadow_factor = je_local_scale->x};
}
)") };

                _defer_light2d_mix_light_effect_pass
                    = { shader::create("!/builtin/defer_light2d_mix_light.shader",
                        R"(
import je.shader;
import je.shader.light2d;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (BACK);

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
    color: float4
};

public func vert(v: vin)
{
    return v2f{
        pos = float4::create(v.vertex, 1.),
        uv = v.uv,
    };
}
public func frag(vf: v2f)
{
    let albedo_buffer   = je_light2d_defer_albedo;
    let self_lumine     = je_light2d_defer_self_luminescence;
    let light_buffer    = uniform_texture:<texture2d>("Light", 0);

    let albedo_color_rgb = pow(texture(albedo_buffer, vf.uv)->xyz, float3::new(2.2, 2.2, 2.2));
    let light_color_rgb = texture(light_buffer, vf.uv)->xyz;
    let self_lumine_color_rgb = texture(self_lumine, vf.uv)->xyz;
    let mixed_color_rgb = max(float3_zero, albedo_color_rgb 
        * (self_lumine_color_rgb + light_color_rgb + float3::new(0.03, 0.03, 0.03)));

    let hdr_color_rgb           = mixed_color_rgb / (mixed_color_rgb + float3::new(1., 1., 1.));
    let hdr_ambient_with_gamma  = pow(hdr_color_rgb, float3::new(1./2.2, 1./2.2, 1./2.2,));

    return fout{
        color = float4::create(hdr_ambient_with_gamma, 1.)
    };
}
)") };
            }

            static DeferLight2DHost* instance(jegl_thread* glcontext)
            {
                static DeferLight2DHost* _instance = nullptr;
                if (_instance == nullptr || _instance->_m_belong_context != glcontext)
                {
                    if (_instance != nullptr)
                        delete _instance;
                    _instance = new DeferLight2DHost(glcontext);
                }
                return _instance;
            }
        };

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

        struct camera_arch
        {
            const Rendqueue* rendqueue;
            const Translation* translation;
            const Projection* projection;
            const Viewport* viewport;
            const RendToFramebuffer* rendToFramebuffer;
            const Light2D::CameraPass* light2DPass;

            bool operator < (const camera_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };

        struct light2d_arch
        {
            const Translation* translation;
            const Light2D::Color* color;
            const Light2D::Shadow* shadow;

            const Shape* shape;
            const Shaders* shaders;
            const Textures* textures;
        };

        struct renderer_arch
        {
            const Rendqueue* rendqueue;
            const Translation* translation;
            const Shape* shape;
            const Shaders* shaders;
            const Textures* textures;

            bool operator < (const renderer_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };

        struct block2d_arch
        {
            const Translation* translation;
            const Light2D::Block* block;
            const Textures* textures;
            const Shape* shape;
        };

        std::vector<camera_arch> m_camera_list;
        std::vector<renderer_arch> m_renderer_list;
        std::list<light2d_arch> m_2dlight_list;
        std::vector<block2d_arch> m_2dblock_list;

        size_t WINDOWS_WIDTH = 0;
        size_t WINDOWS_HEIGHT = 0;

        jeecs::basic::resource<jeecs::graphic::uniformbuffer> m_default_uniform_buffer;
        jeecs::basic::resource<jeecs::graphic::uniformbuffer> m_light2d_uniform_buffer;
        struct default_uniform_buffer_data_t
        {
            jeecs::math::vec4 time;
        };
        struct light2d_uniform_buffer_data_t
        {
            struct light2d_info
            {
                jeecs::math::vec4 color;
                jeecs::math::vec4 position;
                jeecs::math::vec4 direction;
                jeecs::math::vec4 factors;
            };

            light2d_info l2ds[JE_MAX_LIGHT2D_COUNT];
        };

        DeferLight2DGraphicPipelineSystem(game_world w)
            : GraphicPipelineInterfaceSystem(w)
        {
            m_default_uniform_buffer = new jeecs::graphic::uniformbuffer(0, sizeof(default_uniform_buffer_data_t));
            m_light2d_uniform_buffer = new jeecs::graphic::uniformbuffer(1, sizeof(light2d_uniform_buffer_data_t));
        }

        ~DeferLight2DGraphicPipelineSystem()
        {

        }

        void PrepareCameras(Projection& projection,
            Translation& translation,
            OrthoProjection* ortho,
            PerspectiveProjection* perspec,
            Clip* clip,
            Viewport* viewport,
            RendToFramebuffer* rendbuf)
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

            jegl_resource* rend_aim_buffer = rendbuf && rendbuf->framebuffer ? rendbuf->framebuffer->resouce() : nullptr;

            size_t
                RENDAIMBUFFER_WIDTH =
                (size_t)llround(
                    (viewport ? viewport->viewport.z : 1.0f) *
                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_width : WINDOWS_WIDTH)),
                RENDAIMBUFFER_HEIGHT =
                (size_t)llround(
                    (viewport ? viewport->viewport.w : 1.0f) *
                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_height : WINDOWS_HEIGHT));

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
        }

        void PreUpdate()
        {
            m_2dlight_list.clear();
            m_2dblock_list.clear();
            m_camera_list.clear();
            m_renderer_list.clear();

            select_from(get_world())
                .exec(&DeferLight2DGraphicPipelineSystem::PrepareCameras).anyof<OrthoProjection, PerspectiveProjection>()
                .exec(
                    [this](Translation& tarns, Projection& projection, Rendqueue* rendqueue, Viewport* cameraviewport, RendToFramebuffer* rendbuf, Light2D::CameraPass* light2dpass)
                    {
                        if (rendqueue != nullptr)
                            update_priority(rendqueue->rend_queue);

                        m_camera_list.emplace_back(
                            camera_arch{
                                rendqueue,&tarns,&projection, cameraviewport, rendbuf, light2dpass
                            }
                        );

                        if (light2dpass != nullptr)
                        {
                            auto* rend_aim_buffer = (rendbuf != nullptr && rendbuf->framebuffer != nullptr)
                                ? rendbuf->framebuffer->resouce()
                                : nullptr;

                            size_t
                                RENDAIMBUFFER_WIDTH =
                                (size_t)llround(
                                    (cameraviewport ? cameraviewport->viewport.z : 1.0f) *
                                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_width : WINDOWS_WIDTH)),
                                RENDAIMBUFFER_HEIGHT =
                                (size_t)llround(
                                    (cameraviewport ? cameraviewport->viewport.w : 1.0f) *
                                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_height : WINDOWS_HEIGHT));

                            bool need_update = light2dpass->defer_rend_aim == nullptr
                                || light2dpass->defer_rend_aim->resouce()->m_raw_framebuf_data->m_width != RENDAIMBUFFER_WIDTH
                                || light2dpass->defer_rend_aim->resouce()->m_raw_framebuf_data->m_height != RENDAIMBUFFER_HEIGHT;
                            if (need_update)
                            {
                                light2dpass->defer_rend_aim
                                    = new jeecs::graphic::framebuffer(RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT,
                                        {
                                            jegl_texture::texture_format::RGBA, // 漫反射颜色
                                            jegl_texture::texture_format(jegl_texture::texture_format::RGBA | jegl_texture::texture_format::COLOR16), // 自发光颜色，用于法线反射或者发光物体的颜色参数，最终混合shader会将此参数用于光照计算
                                            jegl_texture::texture_format(jegl_texture::texture_format::RGBA | jegl_texture::texture_format::COLOR16), // 视空间坐标(RGB) Alpha通道暂时留空
                                            jegl_texture::texture_format::DEPTH, // 深度缓冲区
                                        });
                                light2dpass->defer_light_effect
                                    = new jeecs::graphic::framebuffer(RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT,
                                        {
                                            (jegl_texture::texture_format)(jegl_texture::texture_format::RGBA | jegl_texture::texture_format::COLOR16), // 光渲染结果
                                        });
                            }
                        }
                    })
                .exec(
                    [this](Translation& trans, Shaders* shads, Textures* texs, Shape* shape, Rendqueue* rendqueue)
                    {
                        // RendOb will be input to a chain and used for swap
                        m_renderer_list.emplace_back(
                            renderer_arch{
                                rendqueue, &trans, shape, shads, texs
                            });
                    }).anyof<Shaders, Textures, Shape>()
                        .except<Light2D::Color>()
                        .exec(
                            [this](Translation& trans,
                                Light2D::Color& color,
                                Light2D::Shadow* shadow,
                                Shape* shape,
                                Shaders* shads,
                                Textures* texs)
                            {
                                m_2dlight_list.emplace_back(
                                    light2d_arch{
                                        &trans, &color, shadow,
                                        shape,
                                        shads,
                                        texs,
                                    }
                                );
                                if (shadow != nullptr)
                                {
                                    bool generate_new_framebuffer =
                                        shadow->shadow_buffer == nullptr
                                        || shadow->shadow_buffer->resouce()->m_raw_framebuf_data->m_width != shadow->resolution_width
                                        || shadow->shadow_buffer->resouce()->m_raw_framebuf_data->m_height != shadow->resolution_height;

                                    if (generate_new_framebuffer)
                                    {
                                        shadow->shadow_buffer = new graphic::framebuffer(
                                            shadow->resolution_width, shadow->resolution_height,
                                            {
                                                jegl_texture::texture_format::RGBA,
                                                // Only store shadow value to R, FBO in opengl not support rend to MONO
                                            }
                                        );
                                        shadow->shadow_buffer->get_attachment(0)->resouce()->m_raw_texture_data->m_sampling
                                            = (jegl_texture::texture_sampling)(
                                                jegl_texture::texture_sampling::LINEAR
                                                | jegl_texture::texture_sampling::CLAMP_EDGE);
                                    }
                                }
                            }
                        ).anyof<Shaders, Textures, Shape>()
                                .exec(
                                    [this](Translation& trans, Light2D::Block& block, Textures* texture, Shape* shape)
                                    {
                                        if (block.mesh.m_block_mesh == nullptr)
                                        {
                                            std::vector<float> _vertex_buffer;
                                            if (!block.mesh.m_block_points.empty())
                                            {
                                                for (auto& point : block.mesh.m_block_points)
                                                {
                                                    _vertex_buffer.insert(_vertex_buffer.end(),
                                                        {
                                                            point.x, point.y, 0.f, 0.f,
                                                            point.x, point.y, 0.f, 1.f,
                                                        });
                                                }
                                                _vertex_buffer.insert(_vertex_buffer.end(),
                                                    {
                                                        block.mesh.m_block_points[0].x, block.mesh.m_block_points[0].y, 0.f, 0.f,
                                                        block.mesh.m_block_points[0].x, block.mesh.m_block_points[0].y, 0.f, 1.f,
                                                    });
                                                block.mesh.m_block_mesh = new jeecs::graphic::vertex(
                                                    jeecs::graphic::vertex::type::TRIANGLESTRIP,
                                                    _vertex_buffer, { 3,1 });
                                            }
                                        }
                                        // Cannot create vertex with 0 point.

                                        if (block.mesh.m_block_mesh != nullptr)
                                        {
                                            m_2dblock_list.push_back(
                                                block2d_arch{
                                                        &trans, &block, texture, shape
                                                }
                                            );
                                        }
                                    }
                            );
                            std::sort(m_2dblock_list.begin(), m_2dblock_list.end(),
                                [](const block2d_arch& a, const block2d_arch& b) {
                                    return a.translation->world_position.z > b.translation->world_position.z;
                                });
                            std::sort(m_camera_list.begin(), m_camera_list.end());
                            std::sort(m_renderer_list.begin(), m_renderer_list.end());
        }

        static void Frame(GraphicPipelineBaseSystem* pipe)
        {
            static_cast<DeferLight2DGraphicPipelineSystem*>(pipe)->DrawFrame();
        }
        void DrawFrame()
        {
            auto* glthread = _m_pipeline->glthread;
            auto* light2d_host = DeferLight2DHost::instance(glthread);

            jegl_get_windows_size(&WINDOWS_WIDTH, &WINDOWS_HEIGHT);

            // TODO: Update shared uniform.
            double current_time = je_clock_time();

            math::vec4 shader_time =
            {
                (float)current_time ,
                (float)abs(2.0 * (current_time * 2.0 - double(int(current_time * 2.0)) - 0.5)) ,
                (float)abs(2.0 * (current_time - double(int(current_time)) - 0.5)),
                (float)abs(2.0 * (current_time / 2.0 - double(int(current_time / 2.0)) - 0.5))
            };

            m_default_uniform_buffer->update_buffer(
                offsetof(default_uniform_buffer_data_t, time),
                sizeof(math::vec4),
                &shader_time);

            light2d_uniform_buffer_data_t l2dbuf = {};
            // Update l2d buffer here.
            size_t light_count = 0;
            for (auto& lightarch : m_2dlight_list)
            {
                if (light_count >= JE_MAX_LIGHT2D_COUNT)
                    break;

                l2dbuf.l2ds[light_count].color = lightarch.color->color;
                l2dbuf.l2ds[light_count].position = lightarch.translation->world_position;
                l2dbuf.l2ds[light_count].direction =
                    lightarch.translation->world_rotation * math::vec3(0.f, -1.f, 1.f).unit();

                l2dbuf.l2ds[light_count].position.w =
                    ((lightarch.shape == nullptr || lightarch.shape->vertex == nullptr
                        ? math::vec3(
                            host()->default_shape_quad->resouce()->m_raw_vertex_data->m_size_x,
                            host()->default_shape_quad->resouce()->m_raw_vertex_data->m_size_y,
                            host()->default_shape_quad->resouce()->m_raw_vertex_data->m_size_z)
                        : math::vec3(
                            lightarch.shape->vertex->resouce()->m_raw_vertex_data->m_size_x,
                            lightarch.shape->vertex->resouce()->m_raw_vertex_data->m_size_y,
                            lightarch.shape->vertex->resouce()->m_raw_vertex_data->m_size_z)
                        )
                        * lightarch.translation->local_scale).length()
                    / 2.0f;

                l2dbuf.l2ds[light_count].factors = math::vec4(
                    lightarch.color->parallel ? 0.f : 1.f,
                    lightarch.color->parallel ? 1.f : 0.f,
                    lightarch.color->decay,
                    0.f);

                if (lightarch.shadow != nullptr)
                {
                    assert(lightarch.shadow->shadow_buffer != nullptr);
                    jegl_using_texture(lightarch.shadow->shadow_buffer->get_attachment(0)->resouce(),
                        JE_SHADOW2D_0 + light_count);
                }
                else
                {
                    jegl_using_texture(light2d_host->_no_shadow->resouce(),
                        JE_SHADOW2D_0 + light_count);
                }

                ++light_count;
            }
            m_light2d_uniform_buffer->update_buffer(
                0,
                sizeof(light2d_uniform_buffer_data_t),
                &l2dbuf
            );

            jegl_using_resource(m_default_uniform_buffer->resouce());
            jegl_using_resource(m_light2d_uniform_buffer->resouce());

            for (auto& current_camera : m_camera_list)
            {
                jegl_resource* rend_aim_buffer = nullptr;
                if (current_camera.rendToFramebuffer)
                {
                    if (current_camera.rendToFramebuffer->framebuffer == nullptr)
                        continue;
                    else
                        rend_aim_buffer = current_camera.rendToFramebuffer->framebuffer->resouce();
                }

                size_t
                    RENDAIMBUFFER_WIDTH = rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_width : WINDOWS_WIDTH,
                    RENDAIMBUFFER_HEIGHT = rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_height : WINDOWS_HEIGHT;

                const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
                const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;

                float MAT4_VP[4][4];
                float MAT4_MV[4][4], MAT4_MVP[4][4];

                math::mat4xmat4(MAT4_VP, MAT4_PROJECTION, MAT4_VIEW);

                // If current camera contain light2d-pass, prepare light shadow here.
                if (current_camera.light2DPass != nullptr)
                {
                    assert(current_camera.light2DPass->defer_rend_aim != nullptr);

                    // Walk throw all light, rend shadows to light's ShadowBuffer.
                    for (auto& lightarch : m_2dlight_list)
                    {
                        if (lightarch.shadow != nullptr)
                        {
                            auto light2d_shadow_aim_buffer = lightarch.shadow->shadow_buffer->resouce();
                            jegl_rend_to_framebuffer(light2d_shadow_aim_buffer, 0, 0,
                                light2d_shadow_aim_buffer->m_raw_framebuf_data->m_width,
                                light2d_shadow_aim_buffer->m_raw_framebuf_data->m_height);

                            jegl_clear_framebuffer(light2d_shadow_aim_buffer);

                            const auto& normal_shadow_pass =
                                lightarch.color->parallel ?
                                light2d_host->_defer_light2d_shadow_parallel_pass :
                                light2d_host->_defer_light2d_shadow_point_pass;
                            const auto& shape_shadow_pass =
                                lightarch.color->parallel ?
                                light2d_host->_defer_light2d_shadow_shape_parallel_pass :
                                light2d_host->_defer_light2d_shadow_shape_point_pass;

                            const auto& sub_shadow_pass = light2d_host->_defer_light2d_shadow_sub_pass;

                            // Let shader know where is the light (through je_color: float4)
                            jegl_using_resource(normal_shadow_pass->resouce());

                            if (lightarch.color->parallel)
                            {
                                jeecs::math::vec3 rotated_light_dir =
                                    lightarch.translation->world_rotation * jeecs::math::vec3(0.f, -1.f, 1.f).unit();
                                jegl_uniform_float4(normal_shadow_pass->resouce(),
                                    normal_shadow_pass->m_builtin->m_builtin_uniform_color,
                                    rotated_light_dir.x,
                                    rotated_light_dir.y,
                                    rotated_light_dir.z,
                                    1.f);
                            }
                            else
                                jegl_uniform_float4(normal_shadow_pass->resouce(),
                                    normal_shadow_pass->m_builtin->m_builtin_uniform_color,
                                    lightarch.translation->world_position.x,
                                    lightarch.translation->world_position.y,
                                    lightarch.translation->world_position.z,
                                    1.f);

                            const size_t block_entity_count = m_2dblock_list.size();

                            std::list<block2d_arch*> block_in_current_layer;

                            auto block2d_iter = m_2dblock_list.begin();
                            auto block2d_end = m_2dblock_list.end();

                            // 看了一圈又一圈，之前这里的代码实在是太晦气了，重写！
                            for (; block2d_iter != block2d_end; ++block2d_iter)
                            {
                                auto& blockarch = *block2d_iter;
                                int64_t current_layer = (int64_t)(blockarch.translation->world_position.z * 100.f);

                                block_in_current_layer.push_back(&blockarch);

                                if (blockarch.block->shadow > 0.f)
                                {
                                    if (lightarch.shadow->shape_offset > 0.f
                                        && current_layer > lightarch.translation->world_position.z * 100.f)
                                    {
                                        // 物体比光源更靠近摄像机，且形状阴影工作被允许

                                        jegl_using_resource(shape_shadow_pass->resouce());
                                        auto* builtin_uniform = shape_shadow_pass->m_builtin;

#define NEED_AND_SET_UNIFORM(ITEM, TYPE, ...) \
do{if (builtin_uniform->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
jegl_uniform_##TYPE(shape_shadow_pass->resouce(), builtin_uniform->m_builtin_uniform_##ITEM, __VA_ARGS__);}while(0)
                                        const float(&MAT4_MODEL)[4][4] = blockarch.translation->object2world;

                                        math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                                        math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                                        NEED_AND_SET_UNIFORM(m, float4x4, MAT4_MODEL);
                                        NEED_AND_SET_UNIFORM(v, float4x4, MAT4_VIEW);
                                        NEED_AND_SET_UNIFORM(p, float4x4, MAT4_PROJECTION);

                                        NEED_AND_SET_UNIFORM(mv, float4x4, MAT4_MV);
                                        NEED_AND_SET_UNIFORM(vp, float4x4, MAT4_VP);
                                        NEED_AND_SET_UNIFORM(mvp, float4x4, MAT4_MVP);

                                        // 通过 local_scale.x 传递阴影权重，.y .z 通道预留
                                        NEED_AND_SET_UNIFORM(local_scale, float3,
                                            blockarch.block->shadow,
                                            0.f,
                                            0.f);

                                        if (blockarch.textures != nullptr)
                                        {
                                            NEED_AND_SET_UNIFORM(tiling, float2, blockarch.textures->tiling.x, blockarch.textures->tiling.y);
                                            NEED_AND_SET_UNIFORM(offset, float2, blockarch.textures->offset.x, blockarch.textures->offset.y);
                                        }

                                        // 通过 je_color 变量传递着色器的位置或方向
                                        if (lightarch.color->parallel)
                                        {
                                            jeecs::math::vec3 rotated_light_dir =
                                                lightarch.translation->world_rotation * jeecs::math::vec3(0.f, -1.f, 0.f);
                                            jegl_uniform_float4(shape_shadow_pass->resouce(),
                                                shape_shadow_pass->m_builtin->m_builtin_uniform_color,
                                                rotated_light_dir.x,
                                                rotated_light_dir.y,
                                                rotated_light_dir.z,
                                                lightarch.shadow->shape_offset);
                                        }
                                        else
                                            jegl_uniform_float4(shape_shadow_pass->resouce(),
                                                shape_shadow_pass->m_builtin->m_builtin_uniform_color,
                                                lightarch.translation->world_position.x,
                                                lightarch.translation->world_position.y,
                                                lightarch.translation->world_position.z,
                                                lightarch.shadow->shape_offset);

                                        if (blockarch.textures != nullptr)
                                        {
                                            jeecs::graphic::texture* main_texture = blockarch.textures->get_texture(0);
                                            if (main_texture != nullptr)
                                                jegl_using_texture(main_texture->resouce(), 0);
                                            else
                                                jegl_using_texture(host()->default_texture->resouce(), 0);
                                        }

                                        jeecs::graphic::vertex* using_shape = (blockarch.shape == nullptr
                                            || blockarch.shape->vertex == nullptr)
                                            ? host()->default_shape_quad
                                            : blockarch.shape->vertex;

                                        jegl_draw_vertex(using_shape->resouce());
#undef NEED_AND_SET_UNIFORM
                                    }
                                    else
                                    {
                                        // 物体比光源更远离摄像机，或者形状阴影被禁用

                                        jegl_using_resource(normal_shadow_pass->resouce());
                                        auto* builtin_uniform = normal_shadow_pass->m_builtin;

#define NEED_AND_SET_UNIFORM(ITEM, TYPE, ...) \
do{if (builtin_uniform->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
jegl_uniform_##TYPE(normal_shadow_pass->resouce(), builtin_uniform->m_builtin_uniform_##ITEM, __VA_ARGS__);}while(0)
                                        const float(&MAT4_MODEL)[4][4] = blockarch.translation->object2world;

                                        math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                                        math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                                        NEED_AND_SET_UNIFORM(m, float4x4, MAT4_MODEL);
                                        NEED_AND_SET_UNIFORM(v, float4x4, MAT4_VIEW);
                                        NEED_AND_SET_UNIFORM(p, float4x4, MAT4_PROJECTION);

                                        NEED_AND_SET_UNIFORM(mv, float4x4, MAT4_MV);
                                        NEED_AND_SET_UNIFORM(vp, float4x4, MAT4_VP);
                                        NEED_AND_SET_UNIFORM(mvp, float4x4, MAT4_MVP);

                                        // 通过 local_scale.x 传递阴影权重，.y .z 通道预留
                                        NEED_AND_SET_UNIFORM(local_scale, float3,
                                            blockarch.block->shadow,
                                            0.f,
                                            0.f);

                                        jegl_draw_vertex(blockarch.block->mesh.m_block_mesh->resouce());
#undef NEED_AND_SET_UNIFORM
                                    }
                                }

                                // 如果下一个阴影将会在不同层级，或者当前阴影是最后一个阴影，则更新阴影覆盖
                                auto next_block2d_iter = block2d_iter + 1;
                                if (next_block2d_iter == block2d_end
                                    || current_layer != (int64_t)(next_block2d_iter->translation->world_position.z * 100.f))
                                {
                                    for (auto* block_in_layer : block_in_current_layer)
                                    {
                                        if (block_in_layer->textures != nullptr)
                                        {
                                            jeecs::graphic::texture* main_texture = block_in_layer->textures->get_texture(0);
                                            if (main_texture != nullptr)
                                                jegl_using_texture(main_texture->resouce(), 0);
                                            else
                                                jegl_using_texture(host()->default_texture->resouce(), 0);
                                        }

                                        jegl_using_resource(sub_shadow_pass->resouce());
                                        auto* builtin_uniform = sub_shadow_pass->m_builtin;

#define NEED_AND_SET_UNIFORM(ITEM, TYPE, ...) \
do{if (builtin_uniform->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
jegl_uniform_##TYPE(sub_shadow_pass->resouce(), builtin_uniform->m_builtin_uniform_##ITEM, __VA_ARGS__);}while(0)
                                        const float(&MAT4_MODEL)[4][4] = block_in_layer->translation->object2world;

                                        math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                                        math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                                        NEED_AND_SET_UNIFORM(m, float4x4, MAT4_MODEL);
                                        NEED_AND_SET_UNIFORM(v, float4x4, MAT4_VIEW);
                                        NEED_AND_SET_UNIFORM(p, float4x4, MAT4_PROJECTION);

                                        NEED_AND_SET_UNIFORM(mv, float4x4, MAT4_MV);
                                        NEED_AND_SET_UNIFORM(vp, float4x4, MAT4_VP);
                                        NEED_AND_SET_UNIFORM(mvp, float4x4, MAT4_MVP);

                                        // local_scale.x .y .z 通道预留
                                        /*NEED_AND_SET_UNIFORM(local_scale, float3,
                                            0.f,
                                            0.f,
                                            0.f);*/

                                        if (block_in_layer->translation->world_position.z < lightarch.translation->world_position.z)
                                            NEED_AND_SET_UNIFORM(color, float4, 1.f, 1.f, 1.f, 1.f);
                                        else
                                            NEED_AND_SET_UNIFORM(color, float4, 0.f, 0.f, 0.f, 0.f);

                                        if (block_in_layer->textures != nullptr)
                                        {
                                            NEED_AND_SET_UNIFORM(tiling, float2, block_in_layer->textures->tiling.x, block_in_layer->textures->tiling.y);
                                            NEED_AND_SET_UNIFORM(offset, float2, block_in_layer->textures->offset.x, block_in_layer->textures->offset.y);
                                        }
                                        jeecs::graphic::vertex* using_shape = (block_in_layer->shape == nullptr
                                            || block_in_layer->shape->vertex == nullptr)
                                            ? host()->default_shape_quad
                                            : block_in_layer->shape->vertex;

                                        jegl_draw_vertex(using_shape->resouce());
#undef NEED_AND_SET_UNIFORM
                                    }
                                    block_in_current_layer.clear();
                                }

                            }
                        }
                    }

                    auto light2d_rend_aim_buffer = current_camera.light2DPass->defer_rend_aim->resouce();
                    jegl_rend_to_framebuffer(light2d_rend_aim_buffer, 0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                    // TODO: Remove this clear for better performance.
                    jegl_clear_framebuffer(light2d_rend_aim_buffer);
                }
                else
                {
                    if (current_camera.viewport)
                        jegl_rend_to_framebuffer(rend_aim_buffer,
                            (size_t)(current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH),
                            (size_t)(current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT),
                            (size_t)(current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH),
                            (size_t)(current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT));
                    else
                        jegl_rend_to_framebuffer(rend_aim_buffer, 0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                    // If camera rend to texture, clear the frame buffer (if need)
                    if (rend_aim_buffer)
                        jegl_clear_framebuffer(rend_aim_buffer);
                }

                // Walk through all entities, rend them to target buffer(include L2DCamera/R2Buf/Screen).
                for (auto& rendentity : m_renderer_list)
                {
                    assert(rendentity.translation);

                    const float(&MAT4_MODEL)[4][4] = rendentity.translation->object2world;

                    math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                    math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                    auto& drawing_shape =
                        (rendentity.shape && rendentity.shape->vertex)
                        ? rendentity.shape->vertex
                        : host()->default_shape_quad;
                    auto& drawing_shaders =
                        (rendentity.shaders && rendentity.shaders->shaders.size())
                        ? rendentity.shaders->shaders
                        : host()->default_shaders_list;

                    // Bind texture here
                    constexpr jeecs::math::vec2 default_tiling(1.f, 1.f), default_offset(0.f, 0.f);
                    const jeecs::math::vec2
                        * _using_tiling = &default_tiling,
                        * _using_offset = &default_offset;
                    jegl_using_texture(host()->default_texture->resouce(), 0);
                    if (rendentity.textures)
                    {
                        _using_tiling = &rendentity.textures->tiling;
                        _using_offset = &rendentity.textures->offset;

                        for (auto& texture : rendentity.textures->textures)
                        {
                            jegl_using_texture(texture.m_texture->resouce(), texture.m_pass_id);
                        }
                    }
                    for (auto& shader_pass : drawing_shaders)
                    {
                        auto* using_shader = &shader_pass;
                        if (!shader_pass->m_builtin)
                            using_shader = &host()->default_shader;

                        jegl_using_resource((*using_shader)->resouce());

                        auto* builtin_uniform = (*using_shader)->m_builtin;
#define NEED_AND_SET_UNIFORM(ITEM, TYPE, ...) \
do{if (builtin_uniform->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
jegl_uniform_##TYPE(shader_pass->resouce(), builtin_uniform->m_builtin_uniform_##ITEM, __VA_ARGS__);}while(0)

                        NEED_AND_SET_UNIFORM(m, float4x4, MAT4_MODEL);
                        NEED_AND_SET_UNIFORM(v, float4x4, MAT4_VIEW);
                        NEED_AND_SET_UNIFORM(p, float4x4, MAT4_PROJECTION);

                        NEED_AND_SET_UNIFORM(mv, float4x4, MAT4_MV);
                        NEED_AND_SET_UNIFORM(vp, float4x4, MAT4_VP);
                        NEED_AND_SET_UNIFORM(mvp, float4x4, MAT4_MVP);

                        NEED_AND_SET_UNIFORM(local_scale, float3,
                            rendentity.translation->local_scale.x,
                            rendentity.translation->local_scale.y,
                            rendentity.translation->local_scale.z);

                        NEED_AND_SET_UNIFORM(tiling, float2, _using_tiling->x, _using_tiling->y);
                        NEED_AND_SET_UNIFORM(offset, float2, _using_offset->x, _using_offset->y);
#undef NEED_AND_SET_UNIFORM
                        jegl_draw_vertex(drawing_shape->resouce());
                    }

                }

                if (current_camera.light2DPass != nullptr)
                {
                    // Rend light buffer to target buffer.
                    assert(current_camera.light2DPass->defer_rend_aim != nullptr
                        && current_camera.light2DPass->defer_light_effect != nullptr);

                    auto* light2d_host = DeferLight2DHost::instance(glthread);

                    // Rend Light result to target buffer.
                    jegl_rend_to_framebuffer(current_camera.light2DPass->defer_light_effect->resouce(), 0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);
                    jegl_clear_framebuffer_color(current_camera.light2DPass->defer_light_effect->resouce());

                    // Bind attachment
                    // 在应用光照时，只需要使用视空间坐标，其他附件均不使用，但是考虑到各种乱七八糟的问题，这里还是都绑定一下
                    // 绑定漫反射颜色通道
                    jegl_using_texture(current_camera.light2DPass->defer_rend_aim->get_attachment(0)->resouce(), JE_LIGHT2D_DEFER_0 + 0);
                    // 绑定自发光通道
                    jegl_using_texture(current_camera.light2DPass->defer_rend_aim->get_attachment(1)->resouce(), JE_LIGHT2D_DEFER_0 + 1);
                    // 绑定视空间坐标通道
                    jegl_using_texture(current_camera.light2DPass->defer_rend_aim->get_attachment(2)->resouce(), JE_LIGHT2D_DEFER_0 + 2);

                    for (auto& light2d : m_2dlight_list)
                    {
                        // 绑定阴影
                        jegl_using_texture(
                            light2d.shadow != nullptr
                            ? light2d.shadow->shadow_buffer->get_attachment(0)->resouce()
                            : light2d_host->_no_shadow->resouce(),
                            JE_LIGHT2D_DEFER_0 + 3);

                        // 开始渲染光照！
                        const float(&MAT4_MODEL)[4][4] = light2d.translation->object2world;

                        math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                        math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                        auto& drawing_shape =
                            (light2d.shape && light2d.shape->vertex)
                            ? light2d.shape->vertex
                            : host()->default_shape_quad;
                        auto& drawing_shaders =
                            (light2d.shaders && light2d.shaders->shaders.size())
                            ? light2d.shaders->shaders
                            : host()->default_shaders_list;

                        // Bind texture here
                        constexpr jeecs::math::vec2 default_tiling(1.f, 1.f), default_offset(0.f, 0.f);
                        const jeecs::math::vec2
                            * _using_tiling = &default_tiling,
                            * _using_offset = &default_offset;
                        jegl_using_texture(host()->default_texture->resouce(), 0);
                        if (light2d.textures)
                        {
                            _using_tiling = &light2d.textures->tiling;
                            _using_offset = &light2d.textures->offset;

                            for (auto& texture : light2d.textures->textures)
                            {
                                jegl_using_texture(texture.m_texture->resouce(), texture.m_pass_id);
                            }
                        }
                        for (auto& shader_pass : drawing_shaders)
                        {
                            auto* using_shader = &shader_pass;
                            if (!shader_pass->m_builtin)
                                using_shader = &host()->default_shader;

                            jegl_using_resource((*using_shader)->resouce());

                            auto* builtin_uniform = (*using_shader)->m_builtin;
#define NEED_AND_SET_UNIFORM(ITEM, TYPE, ...) \
do{if (builtin_uniform->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
jegl_uniform_##TYPE(shader_pass->resouce(), builtin_uniform->m_builtin_uniform_##ITEM, __VA_ARGS__);}while(0)

                            NEED_AND_SET_UNIFORM(m, float4x4, MAT4_MODEL);
                            NEED_AND_SET_UNIFORM(v, float4x4, MAT4_VIEW);
                            NEED_AND_SET_UNIFORM(p, float4x4, MAT4_PROJECTION);

                            NEED_AND_SET_UNIFORM(mv, float4x4, MAT4_MV);
                            NEED_AND_SET_UNIFORM(vp, float4x4, MAT4_VP);
                            NEED_AND_SET_UNIFORM(mvp, float4x4, MAT4_MVP);

                            NEED_AND_SET_UNIFORM(local_scale, float3,
                                light2d.translation->local_scale.x,
                                light2d.translation->local_scale.y,
                                light2d.translation->local_scale.z);

                            NEED_AND_SET_UNIFORM(tiling, float2, _using_tiling->x, _using_tiling->y);
                            NEED_AND_SET_UNIFORM(offset, float2, _using_offset->x, _using_offset->y);

                            // 传入Light2D所需的颜色、衰减信息
                            NEED_AND_SET_UNIFORM(color, float4,
                                light2d.color->color.x,
                                light2d.color->color.y,
                                light2d.color->color.z,
                                light2d.color->color.w);

                            NEED_AND_SET_UNIFORM(shadow2d_resolution, float2,
                                (float)light2d.shadow->resolution_width,
                                (float)light2d.shadow->resolution_height);

                            NEED_AND_SET_UNIFORM(light2d_decay, float, light2d.color->decay);

#undef NEED_AND_SET_UNIFORM
                            jegl_draw_vertex(drawing_shape->resouce());
                        }
                    }

                    // Rend final result color to screen.
                    // Set target buffer.
                    if (current_camera.viewport)
                        jegl_rend_to_framebuffer(rend_aim_buffer,
                            (size_t)(current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH),
                            (size_t)(current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT),
                            (size_t)(current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH),
                            (size_t)(current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT));
                    else
                        jegl_rend_to_framebuffer(rend_aim_buffer, 0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                    // If camera rend to texture, clear the frame buffer (if need)
                    if (rend_aim_buffer)
                        jegl_clear_framebuffer(rend_aim_buffer);

                    jegl_using_resource(light2d_host->_defer_light2d_mix_light_effect_pass->resouce());

                    // 将光照信息储存到通道0，进行最终混合和gamma矫正等操作，完成输出
                    jegl_using_texture(current_camera.light2DPass->defer_light_effect->get_attachment(0)->resouce(), 0);
                    jegl_draw_vertex(light2d_host->_screen_vertex->resouce());

                } // Finish for Light2d effect.                    

            }

            // Redirect to screen space for imgui rend.
            jegl_rend_to_framebuffer(nullptr, 0, 0, WINDOWS_WIDTH, WINDOWS_HEIGHT);
        }

    };

    struct FrameAnimation2DSystem : public game_system
    {
        double _fixed_time = 0.;

        FrameAnimation2DSystem(game_world w)
            : game_system(w)
        {

        }

        void Update()
        {
            _fixed_time += delta_dtime();
            select_from(get_world())
                .exec([this](game_entity e, Animation2D::FrameAnimation& frame_animation, Renderer::Shaders* shaders)
                    {
                        auto* active_animation_frames =
                            frame_animation.animations.m_animations.find(frame_animation.currnet_state.current_animation);
                        if (active_animation_frames != frame_animation.animations.m_animations.end()
                            && active_animation_frames->v.frames.empty() == false)
                        {
                            // 当前动画数据找到，如果当前帧是 SIZEMAX，或者已经到了要更新帧的时候，
                            if (frame_animation.currnet_state.current_frame_index == SIZE_MAX
                                || frame_animation.currnet_state.next_update_time <= _fixed_time)
                            {
                                bool finish_animation = false;

                                auto update_and_apply_component_frame_data =
                                    [](const game_entity& e, jeecs::Animation2D::FrameAnimation::animation_data_set::frame_data& frame)
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
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::INT:
                                            if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<int>(nullptr))
                                            {
                                                jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'int', but member is '%s'.",
                                                    cdata.m_component_type->m_typename,
                                                    cdata.m_member_info->m_member_name,
                                                    cdata.m_member_info->m_member_type->m_typename);
                                                cdata.m_member_addr_cache = nullptr;
                                            }
                                            break;
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::FLOAT:
                                            if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<float>(nullptr))
                                            {
                                                jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'float', but member is '%s'.",
                                                    cdata.m_component_type->m_typename,
                                                    cdata.m_member_info->m_member_name,
                                                    cdata.m_member_info->m_member_type->m_typename);
                                                cdata.m_member_addr_cache = nullptr;
                                            }
                                            break;
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::VEC2:
                                            if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec2>(nullptr))
                                            {
                                                jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'vec2', but member is '%s'.",
                                                    cdata.m_component_type->m_typename,
                                                    cdata.m_member_info->m_member_name,
                                                    cdata.m_member_info->m_member_type->m_typename);
                                                cdata.m_member_addr_cache = nullptr;
                                            }
                                            break;
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::VEC3:
                                            if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec3>(nullptr))
                                            {
                                                jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'vec3', but member is '%s'.",
                                                    cdata.m_component_type->m_typename,
                                                    cdata.m_member_info->m_member_name,
                                                    cdata.m_member_info->m_member_type->m_typename);
                                                cdata.m_member_addr_cache = nullptr;
                                            }
                                            break;
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::VEC4:
                                            if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::vec4>(nullptr))
                                            {
                                                jeecs::debug::logerr("Cannot apply animation frame data for component '%s''s member '%s', type should be 'vec4', but member is '%s'.",
                                                    cdata.m_component_type->m_typename,
                                                    cdata.m_member_info->m_member_name,
                                                    cdata.m_member_info->m_member_type->m_typename);
                                                cdata.m_member_addr_cache = nullptr;
                                            }
                                            break;
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::QUAT4:
                                            if (cdata.m_member_info->m_member_type != jeecs::typing::type_info::of<math::quat>(nullptr))
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
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::INT:
                                            if (cdata.m_offset_mode)
                                                *(int*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.i32;
                                            else
                                                *(int*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.i32;
                                            break;
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::FLOAT:
                                            if (cdata.m_offset_mode)
                                                *(float*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.f32;
                                            else
                                                *(float*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.f32;
                                            break;
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::VEC2:
                                            if (cdata.m_offset_mode)
                                                *(math::vec2*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.v2;
                                            else
                                                *(math::vec2*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.v2;
                                            break;
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::VEC3:
                                            if (cdata.m_offset_mode)
                                                *(math::vec3*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.v3;
                                            else
                                                *(math::vec3*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.v3;
                                            break;
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::VEC4:
                                            if (cdata.m_offset_mode)
                                                *(math::vec4*)cdata.m_member_addr_cache += cdata.m_member_value.m_value.v4;
                                            else
                                                *(math::vec4*)cdata.m_member_addr_cache = cdata.m_member_value.m_value.v4;
                                            break;
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::QUAT4:
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

                                if (frame_animation.currnet_state.current_frame_index == SIZE_MAX)
                                {
                                    frame_animation.currnet_state.current_frame_index = 0;
                                    frame_animation.currnet_state.next_update_time =
                                        _fixed_time + active_animation_frames->v.frames[frame_animation.currnet_state.current_frame_index].m_frame_time;
                                }
                                else
                                {
                                    // 到达下一次更新时间！检查间隔时间，并跳转到对应的帧
                                    auto current_animation_frame_count = active_animation_frames->v.frames.size();

                                    auto delta_time_between_frams = _fixed_time - frame_animation.currnet_state.next_update_time;
                                    auto next_frame_index = (frame_animation.currnet_state.current_frame_index + 1) % current_animation_frame_count;

                                    while (delta_time_between_frams > active_animation_frames->v.frames[next_frame_index].m_frame_time)
                                    {
                                        if (frame_animation.loop == false && next_frame_index == 0)
                                            break;

                                        // 在此应用跳过帧的deltaframe数据
                                        update_and_apply_component_frame_data(e, active_animation_frames->v.frames[next_frame_index]);

                                        delta_time_between_frams -= active_animation_frames->v.frames[next_frame_index].m_frame_time;
                                        next_frame_index = (next_frame_index + 1) % current_animation_frame_count;
                                    }

                                    if (frame_animation.loop == false && next_frame_index == 0)
                                    {
                                        // 帧动画播放完毕，最后更新到最后一帧，然后终止动画
                                        finish_animation = true;
                                        next_frame_index = current_animation_frame_count - 1;
                                    }

                                    frame_animation.currnet_state.current_frame_index = next_frame_index;
                                    frame_animation.currnet_state.next_update_time =
                                        _fixed_time + active_animation_frames->v.frames[frame_animation.currnet_state.current_frame_index].m_frame_time - delta_time_between_frams;
                                }

                                auto& updating_frame = active_animation_frames->v.frames[frame_animation.currnet_state.current_frame_index];
                                update_and_apply_component_frame_data(e, updating_frame);

                                if (shaders != nullptr)
                                {
                                    for (auto& udata : updating_frame.m_uniform_data)
                                    {
                                        switch (udata.m_uniform_value.m_type)
                                        {
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::INT:
                                            shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.i32);
                                            break;
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::FLOAT:
                                            shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.f32);
                                            break;
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::VEC2:
                                            shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.v2);
                                            break;
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::VEC3:
                                            shaders->set_uniform(udata.m_uniform_name.c_str(), udata.m_uniform_value.m_value.v3);
                                            break;
                                        case Animation2D::FrameAnimation::animation_data_set::frame_data::data_value::type::VEC4:
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
                                    frame_animation.currnet_state.set_animation("");
                                }
                            }
                        }
                        // 如果没有找到对应的动画，那么不做任何事。
                        // 这个注释写在这里单纯是因为花括号写得太难看，稍微避免出现一个大于号
                    }
            );
        }
    };
}

jegl_thread* jedbg_get_editing_graphic_thread(void* universe)
{
    return jeecs::GraphicThreadHost::get_default_graphic_pipeline_instance(universe)->glthread;
}
