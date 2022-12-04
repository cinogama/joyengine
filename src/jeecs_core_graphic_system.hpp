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

const char* shader_light2d_path = "je/shader/light2d.wo";
const char* shader_light2d_src = R"(
// JoyEngineECS RScene shader for light2d-system
// This script only used for forward light2d pipeline.

import je.shader;

public let MAX_SHADOW_LIGHT_COUNT = 16;

// define struct for Light
GRAPHIC_STRUCT Light2D
{
    color:      float4,  // color->xyz is color, color->w is intensity.
    position:   float4,  
    direction:  float4,  // direction->xyz is dir, direction->w is angle in radians.
    _padding:   float4,
};

UNIFORM_BUFFER JOYENGINE_LIGHT2D = 1
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
        shadows->add(uniform_texture:<texture2d>("JE_SHADOW2D_{i}", 16 + i));

    return shadows->toarray;
}();

)";

namespace jeecs
{
    struct GraphicThreadHost
    {
        JECS_DISABLE_MOVE_AND_COPY(GraphicThreadHost);

        jegl_thread* glthread = nullptr;

        basic::resource<graphic::vertex> default_shape_quad;
        basic::resource<graphic::shader> default_shader;
        basic::resource<graphic::texture> default_texture;
        jeecs::vector<basic::resource<graphic::shader>> default_shaders_list;

        GraphicThreadHost()
        {
            default_shape_quad =
                new graphic::vertex(jegl_vertex::QUADS,
                    {
                        -0.5f, -0.5f, 0.0f,     0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
                        0.5f, -0.5f, 0.0f,      1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
                        0.5f, 0.5f, 0.0f,       1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
                        -0.5f, 0.5f, 0.0f,      0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
                    },
                    { 3, 2, 3 });

            default_texture = new graphic::texture(2, 2, jegl_texture::texture_format::RGBA);
            default_texture->pix(0, 0).set({ 1.f, 0.f, 1.f, 1.f });
            default_texture->pix(1, 1).set({ 1.f, 0.f, 1.f, 1.f });
            default_texture->pix(0, 1).set({ 0.f, 0.f, 0.f, 1.f });
            default_texture->pix(1, 0).set({ 0.f, 0.f, 0.f, 1.f });

            default_shader = new graphic::shader("je/builtin_default.shader", R"(
// Default shader
import je.shader;

VAO_STRUCT vin {
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
                jegl_using_opengl_apis,
                [](void* ptr, jegl_thread* glthread)
                {
                    ((GraphicThreadHost*)ptr)->_m_rend_update_func(glthread);
                }, this);
        }

        ~GraphicThreadHost()
        {
            assert(this == _m_instance);
            _m_instance = nullptr;
            if (glthread)
                jegl_terminate_graphic_thread(glthread);
        }

        inline static std::atomic<GraphicThreadHost*> _m_instance = nullptr;
        inline static std::atomic<void*> _m_rending_world = nullptr;
        std::function<void(jegl_thread*)> _m_rend_update_func;

        static GraphicThreadHost* get_default_graphic_pipeline_instance(game_universe universe)
        {
            if (nullptr == _m_instance)
            {
                _m_instance = new GraphicThreadHost();
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

        bool IsActive(game_world world)const noexcept
        {
            return world.handle() == _m_rending_world;
        }
        void DeActive(game_world world)const noexcept
        {
            void* excpet_world = world.handle();
            _m_rending_world.compare_exchange_weak(excpet_world, nullptr);
        }

        template<typename PipelineSystemT>
        inline void UpdateFrame(game_world world, PipelineSystemT* sys)noexcept
        {
            void* _null = nullptr;
            if (_m_rending_world.compare_exchange_weak(_null, world.handle()))
            {
                // Rend world update, repleace frame function;
                _m_rend_update_func = [sys](jegl_thread* gt) {sys->Frame(gt); };
            }

            if (glthread && IsActive(world))
                if (!jegl_update(glthread))
                {
                    // update is not work now, means graphic thread want to exit..
                    // ready to shutdown current universe

                    if (game_universe universe = world.get_universe())
                        universe.stop();
                }
        }
    };

    struct EmptyGraphicPipelineSystem : public game_system
    {
        GraphicThreadHost* _m_pipeline;

        EmptyGraphicPipelineSystem(game_world w)
            : game_system(w)
            , _m_pipeline(GraphicThreadHost::get_default_graphic_pipeline_instance(w.get_universe()))
        {

        }
        ~EmptyGraphicPipelineSystem()
        {
            _m_pipeline->DeActive(get_world());
        }

        inline GraphicThreadHost* host()const noexcept
        {
            return _m_pipeline;
        }

        template<typename SysT>
        inline void UpdateFrame(SysT* _this) noexcept
        {
            _m_pipeline->UpdateFrame(get_world(), _this);
        }

        void Frame(jegl_thread* glthread)
        {
            jeecs::debug::logerr("No graphic frame pipeline found in current graphic system(%p), please check.",
                this);
        }
    };

    struct DefaultGraphicPipelineSystem : public EmptyGraphicPipelineSystem
    {
        DefaultGraphicPipelineSystem(game_world w)
            : EmptyGraphicPipelineSystem(w)
        {
        }

        ~DefaultGraphicPipelineSystem()
        {
        }

        void LateUpdate()
        {
            UpdateFrame(this);
        }
    };

    // TODO: When develop defer light2d pipeline disable it tmp...
    //    struct DefaultGraphicPipelineSystem : public EmptyGraphicPipelineSystem
    //    {
    //        using Translation = Transform::Translation;
    //
    //        using Rendqueue = Renderer::Rendqueue;
    //
    //        using Clip = Camera::Clip;
    //        using PerspectiveProjection = Camera::PerspectiveProjection;
    //        using OrthoProjection = Camera::OrthoProjection;
    //        using Projection = Camera::Projection;
    //        using Viewport = Camera::Viewport;
    //        using RendToFramebuffer = Camera::RendToFramebuffer;
    //
    //        using Shape = Renderer::Shape;
    //        using Shaders = Renderer::Shaders;
    //        using Textures = Renderer::Textures;
    //
    //        struct camera_arch
    //        {
    //            const Rendqueue* rendqueue;
    //            const Projection* projection;
    //            const Viewport* viewport;
    //            const RendToFramebuffer* rendToFramebuffer;
    //
    //            bool operator < (const camera_arch& another) const noexcept
    //            {
    //                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
    //                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
    //                return a_queue > b_queue;
    //            }
    //        };
    //        struct renderer_arch
    //        {
    //            const Rendqueue* rendqueue;
    //            const Translation* translation;
    //            const Shape* shape;
    //            const Shaders* shaders;
    //            const Textures* textures;
    //
    //            bool operator < (const renderer_arch& another) const noexcept
    //            {
    //                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
    //                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
    //                return a_queue > b_queue;
    //            }
    //        };
    //
    //        std::priority_queue<camera_arch> m_camera_list;
    //        std::priority_queue<renderer_arch> m_renderer_list;
    //
    //        size_t WINDOWS_WIDTH = 0;
    //        size_t WINDOWS_HEIGHT = 0;
    //
    //        jeecs::basic::resource<jeecs::graphic::uniformbuffer> m_default_uniform_buffer;
    //        struct default_uniform_buffer_data_t
    //        {
    //            jeecs::math::vec4 time;
    //        };
    //
    //        DefaultGraphicPipelineSystem(game_world w)
    //            : EmptyGraphicPipelineSystem(w)
    //        {
    //            m_default_uniform_buffer = new jeecs::graphic::uniformbuffer(0, sizeof(default_uniform_buffer_data_t));
    //        }
    //
    //        ~DefaultGraphicPipelineSystem()
    //        {
    //
    //        }
    //
    //        void PrepareCameras(Projection& projection,
    //            Translation& translation,
    //            OrthoProjection* ortho,
    //            PerspectiveProjection* perspec,
    //            Clip* clip,
    //            Viewport* viewport,
    //            RendToFramebuffer* rendbuf)
    //        {
    //            float mat_inv_rotation[4][4];
    //            translation.world_rotation.create_inv_matrix(mat_inv_rotation);
    //            float mat_inv_position[4][4] = {};
    //            mat_inv_position[0][0] = mat_inv_position[1][1] = mat_inv_position[2][2] = mat_inv_position[3][3] = 1.0f;
    //            mat_inv_position[3][0] = -translation.world_position.x;
    //            mat_inv_position[3][1] = -translation.world_position.y;
    //            mat_inv_position[3][2] = -translation.world_position.z;
    //
    //            // TODO: Optmize
    //            math::mat4xmat4(projection.view, mat_inv_rotation, mat_inv_position);
    //
    //            assert(ortho || perspec);
    //            float znear = clip ? clip->znear : 0.3f;
    //            float zfar = clip ? clip->zfar : 1000.0f;
    //
    //            jegl_resource* rend_aim_buffer = rendbuf && rendbuf->framebuffer ? rendbuf->framebuffer->resouce() : nullptr;
    //
    //            size_t
    //                RENDAIMBUFFER_WIDTH =
    //                (size_t)llround(
    //                    (viewport ? viewport->viewport.z : 1.0f) *
    //                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_width : WINDOWS_WIDTH)),
    //                RENDAIMBUFFER_HEIGHT =
    //                (size_t)llround(
    //                    (viewport ? viewport->viewport.w : 1.0f) *
    //                    (rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_height : WINDOWS_HEIGHT));
    //
    //            if (ortho)
    //            {
    //                graphic::ortho_projection(projection.projection,
    //                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
    //                    ortho->scale, znear, zfar);
    //                graphic::ortho_inv_projection(projection.inv_projection,
    //                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
    //                    ortho->scale, znear, zfar);
    //            }
    //            else
    //            {
    //                graphic::perspective_projection(projection.projection,
    //                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
    //                    perspec->angle, znear, zfar);
    //                graphic::perspective_inv_projection(projection.inv_projection,
    //                    (float)RENDAIMBUFFER_WIDTH, (float)RENDAIMBUFFER_HEIGHT,
    //                    perspec->angle, znear, zfar);
    //            }
    //        }
    //
    //        void PreUpdate()
    //        {
    //            if (!_m_pipeline->IsActive(get_world()))
    //                return;
    //
    //            select_from(get_world())
    //                .exec(&DefaultGraphicPipelineSystem::PrepareCameras).anyof<OrthoProjection, PerspectiveProjection>()
    //                .exec(
    //                    [this](Projection& projection, Rendqueue* rendqueue, Viewport* cameraviewport, RendToFramebuffer* rendbuf)
    //                    {
    //                        // Calc camera proj matrix
    //                        m_camera_list.emplace(
    //                            camera_arch{
    //                                rendqueue, &projection, cameraviewport, rendbuf
    //                            }
    //                        );
    //                    })
    //                .exec(
    //                    [this](Translation& trans, Shaders* shads, Textures* texs, Shape* shape, Rendqueue* rendqueue)
    //                    {
    //                        // TODO: Need Impl AnyOf
    //                            // RendOb will be input to a chain and used for swap
    //                        m_renderer_list.emplace(
    //                            renderer_arch{
    //                                rendqueue, &trans, shape, shads, texs
    //                            });
    //                    }).anyof<Shaders, Textures, Shape>()
    //                        .exec(
    //                            [this](Translation& trans,
    //                                Light2D::Color& color,
    //                                Light2D::Point* point,
    //                                Light2D::Parallel* parallel,
    //                                Light2D::Shadow* shadow)
    //                            {
    //
    //                            }).anyof<Light2D::Point, Light2D::Parallel>();
    //        }
    //        void LateUpdate()
    //        {
    //            UpdateFrame(this);
    //        }
    //
    //        void Frame(jegl_thread* glthread)
    //        {
    //            std::list<renderer_arch> m_renderer_entities;
    //
    //            while (!m_renderer_list.empty())
    //            {
    //                m_renderer_entities.push_back(m_renderer_list.top());
    //                m_renderer_list.pop();
    //            }
    //            jegl_get_windows_size(&WINDOWS_WIDTH, &WINDOWS_HEIGHT);
    //
    //            // Clear frame buffer, (TODO: Only clear depth)
    //            jegl_clear_framebuffer(nullptr);
    //
    //            // TODO: Update shared uniform.
    //            double current_time = je_clock_time();
    //
    //            math::vec4 shader_time =
    //            {
    //                (float)current_time ,
    //                (float)abs(2.0 * (current_time * 2.0 - double(int(current_time * 2.0)) - 0.5)) ,
    //                (float)abs(2.0 * (current_time - double(int(current_time)) - 0.5)),
    //                (float)abs(2.0 * (current_time / 2.0 - double(int(current_time / 2.0)) - 0.5))
    //            };
    //
    //            m_default_uniform_buffer->update_buffer(
    //                offsetof(default_uniform_buffer_data_t, time),
    //                sizeof(math::vec4),
    //                &shader_time);
    //
    //            jegl_using_resource(m_default_uniform_buffer->resouce());
    //
    //            for (; !m_camera_list.empty(); m_camera_list.pop())
    //            {
    //                auto& current_camera = m_camera_list.top();
    //                {
    //                    jegl_resource* rend_aim_buffer = nullptr;
    //                    if (current_camera.rendToFramebuffer)
    //                    {
    //                        if (current_camera.rendToFramebuffer->framebuffer == nullptr
    //                            || !current_camera.rendToFramebuffer->framebuffer->enabled())
    //                            continue;
    //                        else
    //                            rend_aim_buffer = current_camera.rendToFramebuffer->framebuffer->resouce();
    //                    }
    //
    //                    size_t
    //                        RENDAIMBUFFER_WIDTH = rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_width : WINDOWS_WIDTH,
    //                        RENDAIMBUFFER_HEIGHT = rend_aim_buffer ? rend_aim_buffer->m_raw_framebuf_data->m_height : WINDOWS_HEIGHT;
    //
    //                    const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
    //                    const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;
    //
    //                    float MAT4_MV[4][4], MAT4_VP[4][4];
    //                    math::mat4xmat4(MAT4_VP, MAT4_PROJECTION, MAT4_VIEW);
    //
    //                    if (current_camera.viewport)
    //                        jegl_rend_to_framebuffer(rend_aim_buffer,
    //                            current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH,
    //                            current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT,
    //                            current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH,
    //                            current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT);
    //                    else
    //                        jegl_rend_to_framebuffer(rend_aim_buffer, 0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);
    //
    //                    // If camera rend to texture, clear the frame buffer (if need)
    //                    if (rend_aim_buffer)
    //                        jegl_clear_framebuffer(rend_aim_buffer);
    //
    //
    //                    // Walk through all entities, rend them to target buffer(include L2DCamera/R2Buf/Screen).
    //                    for (auto& rendentity : m_renderer_entities)
    //                    {
    //                        assert(rendentity.translation);
    //
    //                        const float(&MAT4_MODEL)[4][4] = rendentity.translation->object2world;
    //
    //                        float MAT4_MVP[4][4];
    //                        math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
    //                        math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);
    //
    //                        auto& drawing_shape =
    //                            (rendentity.shape && rendentity.shape->vertex)
    //                            ? rendentity.shape->vertex
    //                            : host()->default_shape_quad;
    //                        auto& drawing_shaders =
    //                            (rendentity.shaders && rendentity.shaders->shaders.size())
    //                            ? rendentity.shaders->shaders
    //                            : host()->default_shaders_list;
    //
    //                        // Bind texture here
    //                        constexpr jeecs::math::vec2 default_tiling(1.f, 1.f), default_offset(0.f, 0.f);
    //                        const jeecs::math::vec2
    //                            * _using_tiling = &default_tiling,
    //                            * _using_offset = &default_offset;
    //
    //                        if (rendentity.textures)
    //                        {
    //                            _using_tiling = &rendentity.textures->tiling;
    //                            _using_offset = &rendentity.textures->offset;
    //
    //                            for (auto& texture : rendentity.textures->textures)
    //                            {
    //                                if (texture.m_texture->enabled())
    //                                    jegl_using_texture(*texture.m_texture, texture.m_pass_id);
    //                                else
    //                                    // Current texture is missing, using default texture instead.
    //                                    jegl_using_texture(*host()->default_texture, texture.m_pass_id);
    //                            }
    //                        }
    //                        for (auto& shader_pass : drawing_shaders)
    //                        {
    //                            auto* using_shader = &shader_pass;
    //                            if (!shader_pass->enabled() || !shader_pass->m_builtin)
    //                                using_shader = &host()->default_shader;
    //
    //                            jegl_using_resource((*using_shader)->resouce());
    //
    //                            auto* builtin_uniform = (*using_shader)->m_builtin;
    //#define NEED_AND_SET_UNIFORM(ITEM, TYPE, ...) \
    //if (builtin_uniform->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
    // jegl_uniform_##TYPE(*shader_pass, builtin_uniform->m_builtin_uniform_##ITEM, __VA_ARGS__)
    //
    //                            NEED_AND_SET_UNIFORM(m, float4x4, MAT4_MODEL);
    //                            NEED_AND_SET_UNIFORM(v, float4x4, MAT4_VIEW);
    //                            NEED_AND_SET_UNIFORM(p, float4x4, MAT4_PROJECTION);
    //
    //                            NEED_AND_SET_UNIFORM(mv, float4x4, MAT4_MV);
    //                            NEED_AND_SET_UNIFORM(vp, float4x4, MAT4_VP);
    //                            NEED_AND_SET_UNIFORM(mvp, float4x4, MAT4_MVP);
    //
    //                            NEED_AND_SET_UNIFORM(tiling, float2, _using_tiling->x, _using_tiling->y);
    //                            NEED_AND_SET_UNIFORM(offset, float2, _using_offset->x, _using_offset->y);
    //
    //#undef NEED_AND_SET_UNIFORM
    //                            jegl_draw_vertex(*drawing_shape);
    //                        }
    //
    //                    }
    //                }
    //            }
    //
    //            // Redirect to screen space for imgui rend.
    //            jegl_rend_to_framebuffer(nullptr, 0, 0, WINDOWS_WIDTH, WINDOWS_HEIGHT);
    //        }
    //
    //    };

    struct DeferLight2DGraphicPipelineSystem : public EmptyGraphicPipelineSystem
    {
        struct DeferLight2DHost
        {
            jegl_thread* _m_belong_context;

            // Used for move rend result to camera's render aim buffer.
            jeecs::basic::resource<jeecs::graphic::texture> _no_shadow;
            jeecs::basic::resource<jeecs::graphic::vertex> _screen_vertex;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_mix_light_effect_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_point_light_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_pass;
            jeecs::basic::resource<jeecs::graphic::shader> _defer_light2d_shadow_sub_pass;
            DeferLight2DHost(jegl_thread* _ctx)
                : _m_belong_context(_ctx)
            {
                using namespace jeecs::graphic;
                _no_shadow = new texture(1, 1, jegl_texture::texture_format::MONO);
                _no_shadow->pix(0, 0).set(math::vec4(0.f, 0.f, 0.f, 0.f));

                _screen_vertex = new vertex(jegl_vertex::vertex_type::QUADS,
                    {
                        -1.f, -1.f, 0.f,    0.f, 0.f,
                        1.f, -1.f, 0.f,     1.f, 0.f,
                        1.f, 1.f, 0.f,      1.f, 1.f,
                        -1.f, 1.f, 0.f,     0.f, 1.f,
                    },
                    { 3, 2 });

                _defer_light2d_shadow_sub_pass
                    = new shader("je/defer_light2d_shadow_sub.shader", R"(
import je.shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (BACK);

VAO_STRUCT vin 
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
)");

                _defer_light2d_shadow_pass
                    = new shader("je/defer_light2d_shadow_main.shader", R"(
import je.shader;
ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (NONE);

VAO_STRUCT vin
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

    let shadow_vdir = normalize(vpos->xyz - light_vpos->xyz) * 2000. * v.factor;
    
    return v2f{pos = je_p * float4::create(vpos->xyz + shadow_vdir, 1.)};   
}

public func frag(vf: v2f)
{
    return fout{shadow_factor = float::new(1.)};
}
)");

                _defer_light2d_mix_light_effect_pass
                    = new shader("je/defer_light2d_non_light.shader",
                        R"(
import je.shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ZERO);
CULL    (BACK);

VAO_STRUCT vin
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
    let albedo_buffer   = uniform_texture:<texture2d>("Albedo", 0);
    let light_buffer    = uniform_texture:<texture2d>("Light", 1);

    let albedo_color_rgb = pow(texture(albedo_buffer, vf.uv)->xyz, float3::new(2.2, 2.2, 2.2)) ;
    let light_color_rgb = texture(light_buffer, vf.uv)->xyz;
    let mixed_color_rgb = albedo_color_rgb * 0.03 + light_color_rgb;

    let hdr_color_rgb           = mixed_color_rgb / (mixed_color_rgb + float3::new(1., 1., 1.));
    let hdr_ambient_with_gamma  = pow(hdr_color_rgb, float3::new(1./2.2, 1./2.2, 1./2.2,));

    return fout{
        color = float4::create(hdr_ambient_with_gamma, 1.)
    };
}
)");
                _defer_light2d_point_light_pass
                    = new shader("je/defer_light2d_point_light.shader",
                        R"(
import je.shader;

ZTEST   (ALWAYS);
ZWRITE  (DISABLE);
BLEND   (ONE, ONE);
CULL    (BACK);

VAO_STRUCT vin
{
    vertex: float3,
    // uv: float2, // We don't care uv, we will use port position as uv.
};

using v2f = struct{
    pos: float4,
    lpos: float3,
};

using fout = struct{
    color: float4
};

public func vert(v: vin)
{
    let pos = je_mvp * float4::create(v.vertex, 1.);
    let lpos = je_mv * float4::new(0., 0., 0., 1.);

    return v2f{
        pos = pos,
        lpos = lpos->xyz,
    };
}

let PI = float::new(3.1415926535897932384626);

func DistributionGGX(N: float3, H: float3, roughness: float)
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

func GeometrySchlickGGX(NdotV: float, roughness: float)
{
    let r = roughness + float_one;
    let k = r * r / float::new(8.);
    
    let nom = NdotV;
    let denom = NdotV * (float_one - k) + k;

    return nom / denom;
}

func GeometrySmith(N: float3, V: float3, L: float3, roughness: float)
{
    let NdotV = max(dot(N, V), float_zero);
    let NdotL = max(dot(N, L), float_zero);

    let ggx1 = GeometrySchlickGGX(NdotL, roughness);
    let ggx2 = GeometrySchlickGGX(NdotV, roughness);

    return ggx1 * ggx2;
}

func FresnelSchlick(cosTheta: float, F0: float3)
{
    return F0 + (float3_one - F0) * pow(float_one - cosTheta, float::new(5.));
}

func multi_sampling_for_bias_shadow(shadow: texture2d, uv: float2)
{
    let mut shadow_factor = float_zero;
    let bias = 2.;

    let bias_weight = [
        (-2., 2., 0.08),    (-1., 2., 0.08),    (0., 2., 0.08),     (1., 2., 0.08),     (2., 2., 0.08),
        (-2., 1., 0.08),    (-1., 1., 0.08),    (0., 1., 0.16),     (1., 1., 0.16),     (2., 1., 0.08),
        (-2., 0., 0.08),    (-1., 0., 0.08),    (0., 0., 0.72),     (1., 0., 0.16),     (2., 0., 0.08),
        (-2., -1., 0.08),   (-1., -1., 0.16),   (0., -1., 0.16),    (1., -1., 0.16),    (2., -1., 0.08),
        (-2., -2., 0.08),   (-1., -2., 0.08),   (0., -2., 0.08),    (1., -2., 0.08),    (2., -2., 0.08),
    ];

    for (let _, (x, y, weight) : bias_weight)
    {
        shadow_factor = shadow_factor + texture(
            shadow, uv + (float2_one / je_tiling) * float2::create(x, y) * bias
        )->x * weight;
        
    }
    return clamp(shadow_factor, 0., 1.);
}

public func frag(vf: v2f)
{
    let albedo_buffer = uniform_texture:<texture2d>("Albedo", 0);
    let vpos_m_buffer = uniform_texture:<texture2d>("VPositionM", 1);
    let vnorm_r_buffer = uniform_texture:<texture2d>("VNormalR", 2);
    let shadow_buffer = uniform_texture:<texture2d>("Shadow", 3);

    let uv = (vf.pos->xy / vf.pos->w + float2::new(1., 1.)) /2.;

    let albedo = pow(texture(albedo_buffer, uv)->xyz, float3::new(2.2, 2.2, 2.2));
    let vpos_m = texture(vpos_m_buffer, uv);
    let vnorm_r = texture(vnorm_r_buffer, uv);
    let shadow_factor = multi_sampling_for_bias_shadow(shadow_buffer, uv);

    // Calculate light results.
    let intensity = je_color->w;
    let N = vnorm_r->xyz;
    let roughness = vnorm_r->w;
    let metallic = vpos_m->w;

    let FVPos = vpos_m->xyz;
    let LVPos = vf.lpos;
    let VL2FDiff = LVPos - FVPos;

    let F0 = lerp(float3::new(0.04, 0.04, 0.04), albedo, metallic);

    let V = normalize(float3_zero - FVPos);
    let L = normalize(VL2FDiff);
    let H = normalize(V + L);

    let distance = length(VL2FDiff);
    let attenuation = float_one / (distance * distance);
    let radiance = intensity * je_color->xyz * attenuation;

    let NDF = DistributionGGX(N, H, roughness);
    let G = GeometrySmith(N, V, L, roughness);
    let F = FresnelSchlick(max(dot(H, V), float_one), F0);

    let nominator = NDF * G * F;
    let denominator = max(dot(N, V), float_zero)
        * max(dot(N, L), float_zero) 
        + float::new(0.001);
    let specular = nominator / denominator;
    
    let kS = F;
    let kD = (float3_one - kS) * (float_one - metallic);
    let NdotL = max(dot(N, L), float_zero);

    let result = (kD * albedo / PI + specular) * radiance * NdotL * (float_one - shadow_factor);

    return fout{
        color = float4::create(result, 0.),
    };
}

)");
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
                return a_queue > b_queue;
            }
        };

        struct light2d_arch
        {
            const Translation* translation;
            const Light2D::Color* color;
            const Light2D::Point* point;
            const Light2D::Parallel* parallel;
            const Light2D::Shadow* shadow;
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
                return a_queue > b_queue;
            }
        };

        struct block2d_arch
        {
            const Translation* translation;
            const Light2D::Block* block;
            const Textures* textures;
            const Shape* shape;
        };

        std::priority_queue<camera_arch> m_camera_list;
        std::priority_queue<renderer_arch> m_renderer_list;
        std::list<light2d_arch> m_2dlight_list;
        std::vector<block2d_arch> m_2dblock_list;

        size_t WINDOWS_WIDTH = 0;
        size_t WINDOWS_HEIGHT = 0;

        jeecs::basic::resource<jeecs::graphic::uniformbuffer> m_default_uniform_buffer;
        struct default_uniform_buffer_data_t
        {
            jeecs::math::vec4 time;
        };

        DeferLight2DGraphicPipelineSystem(game_world w)
            : EmptyGraphicPipelineSystem(w)
        {
            m_default_uniform_buffer = new jeecs::graphic::uniformbuffer(0, sizeof(default_uniform_buffer_data_t));
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
            if (!_m_pipeline->IsActive(get_world()))
                return;

            m_2dlight_list.clear();
            m_2dblock_list.clear();
            select_from(get_world())
                .exec(&DeferLight2DGraphicPipelineSystem::PrepareCameras).anyof<OrthoProjection, PerspectiveProjection>()
                .exec(
                    [this](Translation& tarns, Projection& projection, Rendqueue* rendqueue, Viewport* cameraviewport, RendToFramebuffer* rendbuf, Light2D::CameraPass* light2dpass)
                    {
                        // Calc camera proj matrix
                        m_camera_list.emplace(
                            camera_arch{
                                rendqueue,&tarns,&projection, cameraviewport, rendbuf, light2dpass
                            }
                        );

                        if (light2dpass != nullptr)
                        {
                            auto* rend_aim_buffer = (rendbuf != nullptr && rendbuf->framebuffer != nullptr && rendbuf->framebuffer->enabled())
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

                            // TODO; Generate & bind shadow buffer.
                            bool need_update = light2dpass->defer_rend_aim == nullptr
                                || light2dpass->defer_rend_aim->resouce()->m_raw_framebuf_data->m_width != RENDAIMBUFFER_WIDTH
                                || light2dpass->defer_rend_aim->resouce()->m_raw_framebuf_data->m_height != RENDAIMBUFFER_HEIGHT;
                            if (need_update)
                            {
                                light2dpass->defer_rend_aim
                                    = new jeecs::graphic::framebuffer(RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT,
                                        {
                                            jegl_texture::texture_format::RGBA, // Albedo
                                            (jegl_texture::texture_format)(jegl_texture::texture_format::RGBA | jegl_texture::texture_format::COLOR16), // VPos_Metallic
                                            (jegl_texture::texture_format)(jegl_texture::texture_format::RGBA | jegl_texture::texture_format::COLOR16), // VNorm_Roughness
                                            jegl_texture::texture_format::DEPTH, //Depth
                                        });
                                light2dpass->defer_light_effect
                                    = new jeecs::graphic::framebuffer(RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT,
                                        {
                                            (jegl_texture::texture_format)(jegl_texture::texture_format::RGBA | jegl_texture::texture_format::COLOR16), // LightResult
                                        });
                            }
                        }
                    })
                .exec(
                    [this](Translation& trans, Shaders* shads, Textures* texs, Shape* shape, Rendqueue* rendqueue)
                    {
                        // TODO: Need Impl AnyOf
                            // RendOb will be input to a chain and used for swap
                        m_renderer_list.emplace(
                            renderer_arch{
                                rendqueue, &trans, shape, shads, texs
                            });
                    }).anyof<Shaders, Textures, Shape>()
                        .exec(
                            [this](Translation& trans,
                                Light2D::Color& color,
                                Light2D::Point* point,
                                Light2D::Parallel* parallel,
                                Light2D::Shadow* shadow)
                            {
                                m_2dlight_list.emplace_back(
                                    light2d_arch{
                                        &trans, &color, point, parallel, shadow
                                    }
                                );
                                if (shadow != nullptr)
                                {
                                    bool generate_new_framebuffer =
                                        shadow->shadow_buffer == nullptr
                                        || !shadow->shadow_buffer->enabled()
                                        || shadow->shadow_buffer->resouce()->m_raw_framebuf_data->m_width != shadow->resolution_width
                                        || shadow->shadow_buffer->resouce()->m_raw_framebuf_data->m_height != shadow->resolution_height;

                                    if (generate_new_framebuffer)
                                    {
                                        shadow->shadow_buffer = new graphic::framebuffer(
                                            shadow->resolution_width, shadow->resolution_height,
                                            {
                                                jegl_texture::texture_format::MONO, // Only store shadow value.
                                            }
                                        );
                                        assert(shadow->shadow_buffer->enabled());
                                        shadow->shadow_buffer->get_attachment(0)->resouce()->m_raw_texture_data->m_sampling
                                            = (jegl_texture::texture_sampling)(
                                                jegl_texture::texture_sampling::LINEAR
                                                | jegl_texture::texture_sampling::CLAMP_EDGE);
                                    }
                                }
                            }
                        ).anyof<Light2D::Point, Light2D::Parallel>()
                                .exec(
                                    [this](Translation& trans, Light2D::Block& block, Textures* texture, Shape* shape)
                                    {
                                        m_2dblock_list.push_back(
                                            block2d_arch{
                                                 &trans, &block, texture, shape
                                            }
                                        );
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
                                    }
                            );
        }
        void Update()
        {
            std::sort(m_2dblock_list.begin(), m_2dblock_list.end(),
                [](const block2d_arch& a, const block2d_arch& b) {
                    return a.translation->world_position.z > b.translation->world_position.z;
                });
        }
        void LateUpdate()
        {
            UpdateFrame(this);
        }

        void Frame(jegl_thread* glthread)
        {
            std::list<renderer_arch> m_renderer_entities;

            while (!m_renderer_list.empty())
            {
                m_renderer_entities.push_back(m_renderer_list.top());
                m_renderer_list.pop();
            }
            jegl_get_windows_size(&WINDOWS_WIDTH, &WINDOWS_HEIGHT);

            // Clear frame buffer, (TODO: Only clear depth)
            jegl_clear_framebuffer(nullptr);

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

            for (; !m_camera_list.empty(); m_camera_list.pop())
            {
                auto& current_camera = m_camera_list.top();
                {
                    jegl_resource* rend_aim_buffer = nullptr;
                    if (current_camera.rendToFramebuffer)
                    {
                        if (current_camera.rendToFramebuffer->framebuffer == nullptr
                            || !current_camera.rendToFramebuffer->framebuffer->enabled())
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

                        auto* light2d_host = DeferLight2DHost::instance(glthread);

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

                                const auto& main_shadow_pass = light2d_host->_defer_light2d_shadow_pass;
                                const auto& sub_shadow_pass = light2d_host->_defer_light2d_shadow_sub_pass;

                                // Let shader know where is the light (through je_color: float4)
                                jegl_using_resource(main_shadow_pass->resouce());
                                jegl_uniform_float4(main_shadow_pass->resouce(),
                                    main_shadow_pass->m_builtin->m_builtin_uniform_color,
                                    lightarch.translation->world_position.x,
                                    lightarch.translation->world_position.y,
                                    lightarch.translation->world_position.z,
                                    1.f);

                                size_t this_depth_layer = SIZE_MAX;
                                const size_t block_entity_count = m_2dblock_list.size();
                                size_t current_entity_id = 0;

                                std::list<block2d_arch*> block_in_current_layer;

                                for (auto& blockarch : m_2dblock_list)
                                {
                                    ++current_entity_id;

                                    // TODO. Ignore the block not in range.

                                    // 1. Prepare m_light_pos/je_mvp
                                    block_in_current_layer.push_back(&blockarch);
                                    {
                                        jegl_using_resource(main_shadow_pass->resouce());
                                        auto* builtin_uniform = main_shadow_pass->m_builtin;

#define NEED_AND_SET_UNIFORM(ITEM, TYPE, ...) \
if (builtin_uniform->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
 jegl_uniform_##TYPE(main_shadow_pass->resouce(), builtin_uniform->m_builtin_uniform_##ITEM, __VA_ARGS__)
                                        const float(&MAT4_MODEL)[4][4] = blockarch.translation->object2world;

                                        math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                                        math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                                        NEED_AND_SET_UNIFORM(m, float4x4, MAT4_MODEL);
                                        NEED_AND_SET_UNIFORM(v, float4x4, MAT4_VIEW);
                                        NEED_AND_SET_UNIFORM(p, float4x4, MAT4_PROJECTION);

                                        NEED_AND_SET_UNIFORM(mv, float4x4, MAT4_MV);
                                        NEED_AND_SET_UNIFORM(vp, float4x4, MAT4_VP);
                                        NEED_AND_SET_UNIFORM(mvp, float4x4, MAT4_MVP);

                                        jegl_draw_vertex(blockarch.block->mesh.m_block_mesh->resouce());
#undef NEED_AND_SET_UNIFORM
                                    }
                                    // 
                                    // 2. Cancel/Cover shadow.
                                    size_t current_layer = (size_t)(blockarch.translation->world_position.z * 100.f);
                                    if (current_entity_id >= block_entity_count
                                        || current_layer != this_depth_layer)
                                    {
                                        this_depth_layer = current_layer;

                                        for (auto* block_in_layer : block_in_current_layer)
                                        {
                                            // Check if the direction A from light to current block and
// the direction B from light to camera. if dot(A,B) > 0, it
// means light at back of block. 
                                            float shadow_factor = 0.f;
                                            //math::vec3 l2bdir =
                                            //    block_in_layer->translation->world_position - lightarch.translation->world_position;
                                            //math::vec3 l2cdir =
                                            //    current_camera.translation->world_position - lightarch.translation->world_position;

                                            //shadow_factor = l2bdir.dot(l2cdir) > 0.f ? 1.f : 0.f;

                                            if (block_in_layer->textures != nullptr)
                                            {
                                                jeecs::graphic::texture* main_texture = block_in_layer->textures->get_texture(0);
                                                if (main_texture != nullptr)
                                                    jegl_using_texture(main_texture->resouce(), 0);
                                            }

                                            jegl_using_resource(sub_shadow_pass->resouce());
                                            auto* builtin_uniform = sub_shadow_pass->m_builtin;

#define NEED_AND_SET_UNIFORM(ITEM, TYPE, ...) \
if (builtin_uniform->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
 jegl_uniform_##TYPE(sub_shadow_pass->resouce(), builtin_uniform->m_builtin_uniform_##ITEM, __VA_ARGS__)
                                            const float(&MAT4_MODEL)[4][4] = block_in_layer->translation->object2world;

                                            math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                                            math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                                            NEED_AND_SET_UNIFORM(m, float4x4, MAT4_MODEL);
                                            NEED_AND_SET_UNIFORM(v, float4x4, MAT4_VIEW);
                                            NEED_AND_SET_UNIFORM(p, float4x4, MAT4_PROJECTION);

                                            NEED_AND_SET_UNIFORM(mv, float4x4, MAT4_MV);
                                            NEED_AND_SET_UNIFORM(vp, float4x4, MAT4_VP);
                                            NEED_AND_SET_UNIFORM(mvp, float4x4, MAT4_MVP);

                                            NEED_AND_SET_UNIFORM(color, float4, shadow_factor, shadow_factor, shadow_factor, shadow_factor);

                                            if (block_in_layer->textures != nullptr)
                                            {
                                                NEED_AND_SET_UNIFORM(tiling, float2, block_in_layer->textures->tiling.x, block_in_layer->textures->tiling.y);
                                                NEED_AND_SET_UNIFORM(offset, float2, block_in_layer->textures->offset.x, block_in_layer->textures->offset.y);
                                            }
                                            jeecs::graphic::vertex* using_shape = (block_in_layer->shape == nullptr
                                                || block_in_layer->shape->vertex == nullptr
                                                || !block_in_layer->shape->vertex->enabled())
                                                ? host()->default_shape_quad
                                                : block_in_layer->shape->vertex;

                                            jegl_draw_vertex(using_shape->resouce());
#undef NEED_AND_SET_UNIFORM
                                        }

                                        // End
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
                                current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH,
                                current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT,
                                current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH,
                                current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT);
                        else
                            jegl_rend_to_framebuffer(rend_aim_buffer, 0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                        // If camera rend to texture, clear the frame buffer (if need)
                        if (rend_aim_buffer)
                            jegl_clear_framebuffer(rend_aim_buffer);
                    }

                    // Walk through all entities, rend them to target buffer(include L2DCamera/R2Buf/Screen).
                    for (auto& rendentity : m_renderer_entities)
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

                        if (rendentity.textures)
                        {
                            _using_tiling = &rendentity.textures->tiling;
                            _using_offset = &rendentity.textures->offset;

                            for (auto& texture : rendentity.textures->textures)
                            {
                                if (texture.m_texture->enabled())
                                    jegl_using_texture(*texture.m_texture, texture.m_pass_id);
                                else
                                    // Current texture is missing, using default texture instead.
                                    jegl_using_texture(*host()->default_texture, texture.m_pass_id);
                            }
                        }
                        for (auto& shader_pass : drawing_shaders)
                        {
                            auto* using_shader = &shader_pass;
                            if (!shader_pass->enabled() || !shader_pass->m_builtin)
                                using_shader = &host()->default_shader;

                            jegl_using_resource((*using_shader)->resouce());

                            auto* builtin_uniform = (*using_shader)->m_builtin;
#define NEED_AND_SET_UNIFORM(ITEM, TYPE, ...) \
if (builtin_uniform->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
 jegl_uniform_##TYPE(*shader_pass, builtin_uniform->m_builtin_uniform_##ITEM, __VA_ARGS__)

                            NEED_AND_SET_UNIFORM(m, float4x4, MAT4_MODEL);
                            NEED_AND_SET_UNIFORM(v, float4x4, MAT4_VIEW);
                            NEED_AND_SET_UNIFORM(p, float4x4, MAT4_PROJECTION);

                            NEED_AND_SET_UNIFORM(mv, float4x4, MAT4_MV);
                            NEED_AND_SET_UNIFORM(vp, float4x4, MAT4_VP);
                            NEED_AND_SET_UNIFORM(mvp, float4x4, MAT4_MVP);

                            NEED_AND_SET_UNIFORM(tiling, float2, _using_tiling->x, _using_tiling->y);
                            NEED_AND_SET_UNIFORM(offset, float2, _using_offset->x, _using_offset->y);
#undef NEED_AND_SET_UNIFORM
                            jegl_draw_vertex(*drawing_shape);
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
                        jegl_using_texture(current_camera.light2DPass->defer_rend_aim->get_attachment(0)->resouce(), 0);
                        jegl_using_texture(current_camera.light2DPass->defer_rend_aim->get_attachment(1)->resouce(), 1);
                        jegl_using_texture(current_camera.light2DPass->defer_rend_aim->get_attachment(2)->resouce(), 2);

                        for (auto& light2d : m_2dlight_list)
                        {
                            if (light2d.shadow != nullptr)
                                jegl_using_texture(light2d.shadow->shadow_buffer->get_attachment(0)->resouce(), 3);
                            else
                                jegl_using_texture(light2d_host->_no_shadow->resouce(), 3);

                            jegl_using_resource(light2d_host->_defer_light2d_point_light_pass->resouce());

                            jegl_uniform_float2(
                                light2d_host->_defer_light2d_point_light_pass->resouce(),
                                light2d_host->_defer_light2d_point_light_pass->m_builtin->m_builtin_uniform_tiling,
                                current_camera.light2DPass->defer_light_effect->resouce()->m_raw_framebuf_data->m_width,
                                current_camera.light2DPass->defer_light_effect->resouce()->m_raw_framebuf_data->m_height
                            );

                            jegl_uniform_float4(light2d_host->_defer_light2d_point_light_pass->resouce(),
                                light2d_host->_defer_light2d_point_light_pass->m_builtin->m_builtin_uniform_color,
                                light2d.color->color.x,
                                light2d.color->color.y,
                                light2d.color->color.z,
                                light2d.color->color.w
                            );

                            auto* builtin_uniform = light2d_host->_defer_light2d_point_light_pass->m_builtin;
#define NEED_AND_SET_UNIFORM(ITEM, TYPE, ...) \
if (builtin_uniform->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
    jegl_uniform_##TYPE(light2d_host->_defer_light2d_point_light_pass->resouce(),\
    builtin_uniform->m_builtin_uniform_##ITEM, __VA_ARGS__)

                            const float(&MAT4_MODEL)[4][4] = light2d.translation->object2world;

                            math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                            math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL);

                            NEED_AND_SET_UNIFORM(m, float4x4, MAT4_MODEL);
                            NEED_AND_SET_UNIFORM(v, float4x4, MAT4_VIEW);
                            NEED_AND_SET_UNIFORM(p, float4x4, MAT4_PROJECTION);

                            NEED_AND_SET_UNIFORM(mv, float4x4, MAT4_MV);
                            NEED_AND_SET_UNIFORM(vp, float4x4, MAT4_VP);
                            NEED_AND_SET_UNIFORM(mvp, float4x4, MAT4_MVP);

#undef NEED_AND_SET_UNIFORM
                            jegl_draw_vertex(light2d_host->_screen_vertex->resouce());
                        }
                        // Rend final result color to screen.
                        // Set target buffer.
                        if (current_camera.viewport)
                            jegl_rend_to_framebuffer(rend_aim_buffer,
                                current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH,
                                current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT,
                                current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH,
                                current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT);
                        else
                            jegl_rend_to_framebuffer(rend_aim_buffer, 0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                        // If camera rend to texture, clear the frame buffer (if need)
                        if (rend_aim_buffer)
                            jegl_clear_framebuffer(rend_aim_buffer);

                        jegl_using_resource(light2d_host->_defer_light2d_mix_light_effect_pass->resouce());

                        // Bind light effect to textre-pass-1, because pass-0 is used for storing
                        jegl_using_texture(current_camera.light2DPass->defer_light_effect->get_attachment(0)->resouce(), 1);

                        jegl_draw_vertex(light2d_host->_screen_vertex->resouce());

                    } // Finish for Light2d effect.                    
                }
            }

            // Redirect to screen space for imgui rend.
            jegl_rend_to_framebuffer(nullptr, 0, 0, WINDOWS_WIDTH, WINDOWS_HEIGHT);
        }

    };
}

jegl_thread* jedbg_get_editing_graphic_thread(void* universe)
{
    return jeecs::GraphicThreadHost::get_default_graphic_pipeline_instance(universe)->glthread;
}

void* jedbg_get_rendering_world(void* universe)
{
    auto* host = jeecs::GraphicThreadHost::get_default_graphic_pipeline_instance(universe);
    return host->_m_rending_world;
}