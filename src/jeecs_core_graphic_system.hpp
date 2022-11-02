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
        using Translation = Transform::Translation;
        using Layer = Transform::Layer;

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
                return a_queue > b_queue;
            }
        };
        struct renderer_arch
        {
            const Rendqueue* rendqueue;
            const Translation* translation;
            const Layer* layer;
            const Shape* shape;
            const Shaders* shaders;
            const Textures* textures;

            bool operator < (const renderer_arch& another) const noexcept
            {
                int a_layer = layer ? layer->layer : 0;
                int b_layer = another.layer ? another.layer->layer : 0;

                if (a_layer == b_layer)
                {
                    int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                    int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                    return a_queue > b_queue;
                }
                return a_layer < b_layer;
            }
        };

        std::priority_queue<camera_arch> m_camera_list;
        std::priority_queue<renderer_arch> m_renderer_list;

        size_t WINDOWS_WIDTH = 0;
        size_t WINDOWS_HEIGHT = 0;

        jeecs::basic::resource<jeecs::graphic::uniformbuffer> m_default_uniform_buffer;
        struct default_uniform_buffer_data_t
        {
            jeecs::math::vec4 time;
            jeecs::math::vec4 layer_and_padding;
        };

        DefaultGraphicPipelineSystem(game_world w)
            : EmptyGraphicPipelineSystem(w)
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
            if (!_m_pipeline->IsActive(get_world()))
                return;

            select_from(get_world())
                .exec(&DefaultGraphicPipelineSystem::PrepareCameras).anyof<OrthoProjection, PerspectiveProjection>()
                .exec(
                    [this](Projection& projection, Rendqueue* rendqueue, Viewport* cameraviewport, RendToFramebuffer* rendbuf)
                    {
                        // Calc camera proj matrix
                        m_camera_list.emplace(
                            camera_arch{
                                rendqueue, &projection, cameraviewport, rendbuf
                            }
                        );
                    })
                .exec(
                    [this](Translation& trans, Layer* layer, Shaders* shads, Textures* texs, Shape* shape, Rendqueue* rendqueue)
                    {
                        // TODO: Need Impl AnyOf
                            // RendOb will be input to a chain and used for swap
                        m_renderer_list.emplace(
                            renderer_arch{
                                rendqueue, &trans, layer, shape, shads, texs
                            });
                    }).anyof<Shaders, Textures, Shape>()
                        .exec(
                            [](Translation& trans,
                                Light2D::Color& color,
                                Light2D::LayerEffect* effect_layer,
                                Light2D::Point* point,
                                Light2D::Parallel* parallel,
                                Light2D::Shadow* shadow)
                            {

                            }).anyof<Light2D::Point, Light2D::Parallel>()
                                ;
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

                    const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
                    const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;

                    float MAT4_MV[4][4], MAT4_VP[4][4];
                    math::mat4xmat4(MAT4_VP, MAT4_PROJECTION, MAT4_VIEW);
                    // TODO: Update camera shared uniform.

                    uint32_t current_rendering_entity_layer_group = typing::INVALID_UINT32;

                    for (auto& rendentity : m_renderer_entities)
                    {
                        uint32_t current_rendering_entity_layer = rendentity.layer == nullptr ? 0 : rendentity.layer->layer;
                        if (current_rendering_entity_layer != current_rendering_entity_layer_group)
                        {
                            // Update uniform block & update light2d shadow buf?
                            current_rendering_entity_layer_group = current_rendering_entity_layer;
                            math::vec4 shader_layer_and_padding =
                            {
                                (float)current_rendering_entity_layer_group, 0.f, 0.f, 0.f
                            };

                            m_default_uniform_buffer->update_buffer(
                                offsetof(default_uniform_buffer_data_t, layer_and_padding),
                                sizeof(math::vec4),
                                &shader_layer_and_padding);
                            jegl_using_resource(m_default_uniform_buffer->resouce());
                        }

                        /*jegl_using_texture();
                        jegl_draw_vertex_with_shader();*/
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
                        if (rendentity.textures)
                            for (auto& texture : rendentity.textures->textures)
                            {
                                if (texture.m_texture->enabled())
                                    jegl_using_texture(*texture.m_texture, texture.m_pass_id);
                                else
                                    // Current texture is missing, using default texture instead.
                                    jegl_using_texture(*host()->default_texture, texture.m_pass_id);
                            }

                        for (auto& shader_pass : drawing_shaders)
                        {
                            auto* using_shader = &shader_pass;
                            if (!shader_pass->enabled() || !shader_pass->m_builtin)
                                using_shader = &host()->default_shader;

                            jegl_using_resource((*using_shader)->resouce());

                            auto* builtin_uniform = (*using_shader)->m_builtin;
#define NEED_AND_SET_UNIFORM(ITEM, TYPE, VALUE) \
if (builtin_uniform->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
 jegl_uniform_##TYPE(*shader_pass, builtin_uniform->m_builtin_uniform_##ITEM, VALUE)

                            NEED_AND_SET_UNIFORM(m, float4x4, MAT4_MODEL);
                            NEED_AND_SET_UNIFORM(v, float4x4, MAT4_VIEW);
                            NEED_AND_SET_UNIFORM(p, float4x4, MAT4_PROJECTION);

                            NEED_AND_SET_UNIFORM(mv, float4x4, MAT4_MV);
                            NEED_AND_SET_UNIFORM(vp, float4x4, MAT4_VP);
                            NEED_AND_SET_UNIFORM(mvp, float4x4, MAT4_MVP);

#undef NEED_AND_SET_UNIFORM
                            jegl_draw_vertex(*drawing_shape);
                        }

                    }
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