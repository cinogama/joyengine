#pragma once

// Important:
//  Graphic is a world-less system, it can be switched from one to another system.
//
// How to switch system?
//  In universe, we can find the system with typeinfo and get current world, a task
//  will be push_front to 'pick-out' system function and re-add it to another world.


#ifndef JE_IMPL
#   define JE_IMPL
#   define JE_ENABLE_DEBUG_API
#   include "jeecs.hpp"
#endif

#include <queue>
#include <list>

namespace jeecs
{
    struct DefaultGraphicPipelineSystem : public game_shared_system
    {
        using Translation = Transform::Translation;

        using Rendqueue = Renderer::Rendqueue;

        using Clip = Camera::Clip;
        using PerspectiveProjection = Camera::PerspectiveProjection;
        using OrthoProjection = Camera::OrthoProjection;
        using Projection = Camera::Projection;
        using Viewport = Camera::Viewport;

        using Shape = Renderer::Shape;
        using Shaders = Renderer::Shaders;
        using Textures = Renderer::Textures;

        jegl_thread* glthread = nullptr;

        basic::resource<graphic::vertex> default_shape_quad;
        basic::resource<graphic::shader> default_shader;
        basic::resource<graphic::texture> default_texture;
        jeecs::vector<basic::resource<graphic::shader>> default_shaders_list;

        DefaultGraphicPipelineSystem(game_universe universe)
            : game_shared_system(universe)
        {
            // GraphicSystem is a public system and not belong to any world.
            default_shape_quad =
                new graphic::vertex(jegl_vertex::QUADS,
                    { -0.5f, -0.5f, 0.0f,     0.0f, 1.0f,
                    0.5f, -0.5f, 0.0f,      1.0f, 1.0f,
                    0.5f, 0.5f, 0.0f,       1.0f, 0.0f,
                    -0.5f, 0.5f, 0.0f,      0.0f, 0.0f, },
                    { 3, 2 });

            default_texture = new graphic::texture(2, 2, jegl_texture::texture_format::RGBA);
            default_texture->pix(0, 0).set({ 1.f, 0.f, 1.f, 1.f });
            default_texture->pix(1, 1).set({ 1.f, 0.f, 1.f, 1.f });
            default_texture->pix(0, 1).set({ 0.f, 0.f, 0.f, 1.f });
            default_texture->pix(1, 0).set({ 0.f, 0.f, 0.f, 1.f });

            default_shader = new graphic::shader("je/builtin_default.shader", R"(
// Default shader
import je.shader;

using VAO_STRUCT vin = struct {
    vertex : float3,
};
using v2f = struct {
    pos : float4,
};
using fout = struct {
    color : float4
};

let vert = \v: vin = v2f{ pos = je_mvp * vertex_pos }
                where vertex_pos = float4(v.vertex, 1.);;

let frag = \f: v2f = fout{ color = float4(t, 0, t, 1) }
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
                {((DefaultGraphicPipelineSystem*)ptr)->Frame(glthread); }, this);
            register_system_func(&DefaultGraphicPipelineSystem::CalculateProjection,
                {
                    any_of<PerspectiveProjection>(),
                    any_of<OrthoProjection>(),
                });
            register_system_func(&DefaultGraphicPipelineSystem::SimplePrepareCamera,
                {
                    before(&DefaultGraphicPipelineSystem::FlushPipeLine),
                    any_of<PerspectiveProjection>(),
                    any_of<OrthoProjection>(),
                });
            register_system_func(&DefaultGraphicPipelineSystem::SimpleRendObject,
                {
                    before(&DefaultGraphicPipelineSystem::FlushPipeLine),
                    any_of<Shaders>(),
                    any_of<Textures>(),
                    any_of<Shape>(),
                });
            register_system_func(&DefaultGraphicPipelineSystem::FlushPipeLine);
        }
        ~DefaultGraphicPipelineSystem()
        {
            if (glthread)
                jegl_terminate_graphic_thread(glthread);
        }

        struct camera_arch
        {
            const Rendqueue* rendqueue;
            const Projection* projection;
            const Viewport* viewport;

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

        std::priority_queue<camera_arch> m_camera_list;
        std::priority_queue<renderer_arch> m_renderer_list;

        size_t WINDOWS_WIDTH = 0;
        size_t WINDOWS_HEIGHT = 0;

        void Frame(jegl_thread* glthread)
        {
            std::list<renderer_arch> m_renderer_entities;

            while (!m_renderer_list.empty())
            {
                m_renderer_entities.push_back(m_renderer_list.top());
                m_renderer_list.pop();
            }
            jegl_get_windows_size(&WINDOWS_WIDTH, &WINDOWS_HEIGHT);
            const size_t RENDAIMBUFFER_WIDTH = WINDOWS_WIDTH, RENDAIMBUFFER_HEIGHT = WINDOWS_HEIGHT;

            // Clear frame buffer, (TODO: Only clear depth)
            jegl_clear_framebuffer(nullptr);

            // TODO: Update shared uniform.
            double current_time = je_clock_time();

            math::vec4 shader_time =
            { (float)current_time ,
            (float)abs(2.0 * (current_time * 2.0 - double(int(current_time * 2.0)) - 0.5)) ,
            (float)abs(2.0 * (current_time - double(int(current_time)) - 0.5)),
            (float)abs(2.0 * (current_time / 2.0 - double(int(current_time / 2.0)) - 0.5)) };

            jegl_update_shared_uniform(0, sizeof(math::vec4), &shader_time);

            while (!m_camera_list.empty())
            {
                auto& current_camera = m_camera_list.top();
                {
                    // TODO: If camera has component named 'RendToTexture' handle it.
                    jegl_resource* rend_aim_buffer = nullptr;

                    if (current_camera.viewport)
                        jegl_rend_to_framebuffer(nullptr,
                            current_camera.viewport->viewport.x * (float)RENDAIMBUFFER_WIDTH,
                            current_camera.viewport->viewport.y * (float)RENDAIMBUFFER_HEIGHT,
                            current_camera.viewport->viewport.z * (float)RENDAIMBUFFER_WIDTH,
                            current_camera.viewport->viewport.w * (float)RENDAIMBUFFER_HEIGHT);
                    else
                        jegl_rend_to_framebuffer(nullptr, 0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                    // If camera rend to texture, clear the frame buffer (if need)
                    if (rend_aim_buffer)
                        jegl_clear_framebuffer(nullptr);

                    const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
                    const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;

                    float MAT4_VP[4][4]; math::mat4xmat4(MAT4_VP, MAT4_PROJECTION, MAT4_VIEW);
                    // TODO: Update camera shared uniform.

                    for (auto& rendentity : m_renderer_entities)
                    {
                        /*jegl_using_texture();
                        jegl_draw_vertex_with_shader();*/
                        assert(rendentity.translation);

                        const float(&MAT4_MODEL)[4][4] = rendentity.translation->object2world;

                        float MAT4_MVP[4][4];  math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                        // TODO: Calc needed matrix and update uniform

                        // float MAT4_MV[4][4]; math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL); ?

                        auto& drawing_shape =
                            (rendentity.shape && rendentity.shape->vertex)
                            ? rendentity.shape->vertex
                            : default_shape_quad;
                        auto& drawing_shaders =
                            (rendentity.shaders && rendentity.shaders->shaders.size())
                            ? rendentity.shaders->shaders
                            : default_shaders_list;

                        // Bind texture here
                        if (rendentity.textures)
                            for (auto& texture : rendentity.textures->textures)
                            {
                                if (texture.m_texture->enabled())
                                    jegl_using_texture(*texture.m_texture, texture.m_pass_id);
                                else
                                    // Current texture is missing, using default texture instead.
                                    jegl_using_texture(*default_texture, texture.m_pass_id);
                            }

                        for (auto& shader_pass : drawing_shaders)
                        {
                            auto* using_shader = &shader_pass;
                            if (!shader_pass->enabled() || !shader_pass->m_builtin)
                                using_shader = &default_shader;

                            jegl_using_resource((*using_shader)->resouce());

                            auto* builtin_uniform = (*using_shader)->m_builtin;
#define NEED_AND_SET_UNIFORM(ITEM, TYPE, VALUE) \
if (builtin_uniform->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
 jegl_uniform_##TYPE(*shader_pass, builtin_uniform->m_builtin_uniform_##ITEM, VALUE)

                            NEED_AND_SET_UNIFORM(m, float4x4, MAT4_MODEL);
                            NEED_AND_SET_UNIFORM(v, float4x4, MAT4_VIEW);
                            NEED_AND_SET_UNIFORM(p, float4x4, MAT4_PROJECTION);

                            NEED_AND_SET_UNIFORM(vp, float4x4, MAT4_VP);
                            NEED_AND_SET_UNIFORM(mvp, float4x4, MAT4_MVP);

#undef NEED_AND_SET_UNIFORM
                            jegl_draw_vertex(*drawing_shape);
                        }

                    }
                }
                m_camera_list.pop();
            }
        }
        void CalculateProjection(
            Projection* projection,
            const Translation* translation,
            maynot<const OrthoProjection*> ortho,
            maynot<const PerspectiveProjection*> perspec,
            maynot<const Clip*> clip)
        {
            float mat_inv_rotation[4][4];
            translation->world_rotation.create_inv_matrix(mat_inv_rotation);
            float mat_inv_position[4][4] = {};
            mat_inv_position[0][0] = mat_inv_position[1][1] = mat_inv_position[2][2] = mat_inv_position[3][3] = 1.0f;
            mat_inv_position[3][0] = -translation->world_position.x;
            mat_inv_position[3][1] = -translation->world_position.y;
            mat_inv_position[3][2] = -translation->world_position.z;

            // TODO: Optmize
            math::mat4xmat4(projection->view, mat_inv_rotation, mat_inv_position);

            assert(ortho || perspec);
            float znear = clip ? clip->znear : 0.3f;
            float zfar = clip ? clip->zfar : 1000.0f;
            if (ortho)
            {
                graphic::ortho_projection(projection->projection,
                    (float)WINDOWS_WIDTH, (float)WINDOWS_HEIGHT,
                    ortho->scale, znear, zfar);
                graphic::ortho_inv_projection(projection->inv_projection,
                    (float)WINDOWS_WIDTH, (float)WINDOWS_HEIGHT,
                    ortho->scale, znear, zfar);
            }
            else
            {
                graphic::perspective_projection(projection->projection,
                    (float)WINDOWS_WIDTH, (float)WINDOWS_HEIGHT,
                    perspec->angle, znear, zfar);
                graphic::perspective_inv_projection(projection->inv_projection,
                    (float)WINDOWS_WIDTH, (float)WINDOWS_HEIGHT,
                    perspec->angle, znear, zfar);
            }
        }

        void SimplePrepareCamera(
            const Projection* projection,
            maynot<const Rendqueue*> rendqueue,
            maynot<const Viewport*> cameraviewport)
        {
            // Calc camera proj matrix
            m_camera_list.emplace(
                camera_arch{
                    rendqueue, projection, cameraviewport
                }
            );
        }

        void SimpleRendObject(
            const Translation* trans,
            maynot<const Shaders*> shads,
            maynot<const Textures*> texs,
            maynot<const Shape*> shape,
            maynot<const Rendqueue*> rendqueue)
        {
            // RendOb will be input to a chain and used for swap
            m_renderer_list.emplace(
                renderer_arch{
                    rendqueue, trans, shape, shads, texs
                });
        }

        void FlushPipeLine()
        {
            if (glthread)
                if (!jegl_update(glthread))
                {
                    // update is not work now, means graphic thread want to exit..
                    // ready to shutdown current universe

                    if (game_universe universe = get_universe())
                        universe.stop();
                }
        }
    };

}