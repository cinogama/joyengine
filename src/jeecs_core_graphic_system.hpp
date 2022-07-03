#pragma once

// Important:
//  Graphic is a world-less system, it can be switched from one to another system.
//
// How to switch system?
//  In universe, we can find the system with typeinfo and get current world, a task
//  will be push_front to 'pick-out' system function and re-add it to another world.


#define JE_IMPL
#include "jeecs.hpp"

#include <queue>
#include <list>

namespace jeecs
{
    struct DefaultGraphicPipelineSystem : public game_system
    {
        using Translation = Transform::Translation;

        using Rendqueue = Renderer::Rendqueue;

        using Clip = Camera::Clip;
        using PerspectiveProjection = Camera::PerspectiveProjection;
        using OrthoProjection = Camera::OrthoProjection;
        using Projection = Camera::Projection;
        using Viewport = Camera::Viewport;

        using Material = Renderer::Material;
        using Shape = Renderer::Shape;

        jegl_thread* glthread = nullptr;
        game_universe current_universe = nullptr;

        basic::resource<graphic::vertex> default_shape_quad;
        basic::resource<graphic::shader> default_shader;

        DefaultGraphicPipelineSystem(game_universe universe)
            : game_system(nullptr)
            , current_universe(universe)
        {
            // GraphicSystem is a public system and not belong to any world.
            default_shape_quad =
                new graphic::vertex(jegl_vertex::QUADS,
                    { -0.5f, -0.5f, 0.0f,     0.0f, 1.0f,
                    0.5f, -0.5f, 0.0f,      1.0f, 1.0f,
                    0.5f, 0.5f, 0.0f,       1.0f, 0.0f,
                    -0.5f, 0.5f, 0.0f,      0.0f, 0.0f, },
                    { 3, 2 });

            default_shader = new graphic::shader("je/default.shader", R"(
// Default shader
import je.shader;

var example = uniform:<float>("XX", float(1.));

func vert(var vdata:vertex_in)
{
    var ipos    = vdata->in:<float3>(0);
    var iuv     = vdata->in:<float2>(1);

    var opos = je_mvp * float4(ipos, example);
    return vertex_out(opos);
}
func frag(var fdata:fragment_in)
{
    var flashing_color = je_time->y();
    return fragment_out(float4(flashing_color, 0, flashing_color, 1));
}

)");

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
                return a_queue < b_queue;
            }
        };
        struct renderer_arch
        {
            const Rendqueue* rendqueue;
            const Translation* translation;
            const Material* material;
            const Shape* shape;

            bool operator < (const renderer_arch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
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
                    if(rend_aim_buffer)
                        jegl_clear_framebuffer(nullptr);

                    const float(&MAT4_VIEW)[4][4] = current_camera.projection->view;
                    const float(&MAT4_PROJECTION)[4][4] = current_camera.projection->projection;

                    float MAT4_VP[4][4]; math::mat4xmat4(MAT4_VP, MAT4_PROJECTION, MAT4_VIEW);
                    // TODO: Update camera shared uniform.

                    for (auto& rendentity : m_renderer_entities)
                    {
                        /*jegl_using_texture();
                        jegl_draw_vertex_with_shader();*/
                        assert(rendentity.material && rendentity.translation);

                        const float(&MAT4_MODEL)[4][4] = rendentity.translation->object2world;

                        float MAT4_MVP[4][4];  math::mat4xmat4(MAT4_MVP, MAT4_VP, MAT4_MODEL);
                        // TODO: Calc needed matrix and update uniform

                        // float MAT4_MV[4][4]; math::mat4xmat4(MAT4_MV, MAT4_VIEW, MAT4_MODEL); ?

                        auto& drawing_shape =
                            (rendentity.shape && rendentity.shape->vertex)
                            ? rendentity.shape->vertex
                            : default_shape_quad;

                        // TODO: Bind texture here

                        //for (auto& shader_pass : rendentity.material->shaders)
                        auto& shader_pass = default_shader;
                        {
                            auto* builtin_uniform = shader_pass->m_builtin;
#define NEED_AND_SET_UNIFORM(ITEM, TYPE, VALUE) \
if (builtin_uniform->m_builtin_uniform_##ITEM != typing::INVALID_UINT32)\
 jegl_uniform_##TYPE(*shader_pass, builtin_uniform->m_builtin_uniform_##ITEM, VALUE)

                            NEED_AND_SET_UNIFORM(m, float4x4, MAT4_MODEL);
                            NEED_AND_SET_UNIFORM(v, float4x4, MAT4_VIEW);
                            NEED_AND_SET_UNIFORM(p, float4x4, MAT4_PROJECTION);

                            NEED_AND_SET_UNIFORM(vp, float4x4, MAT4_VP);
                            NEED_AND_SET_UNIFORM(mvp, float4x4, MAT4_MVP);

#undef NEED_AND_SET_UNIFORM
                            jegl_draw_vertex_with_shader(*drawing_shape, *shader_pass);
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
            math::mat4xmat4(projection->view, mat_inv_position, mat_inv_rotation);

            assert(ortho || perspec);
            float znear = clip ? clip->znear : 0.3f;
            float zfar = clip ? clip->zfar : 1000.0f;
            if (ortho)
            {
                graphic::ortho_projection(projection->projection,
                    (float)WINDOWS_WIDTH, (float)WINDOWS_HEIGHT,
                    ortho->scale, znear, zfar);
            }
            else
            {
                graphic::perspective_projection(projection->projection,
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
            const Material* mat,
            maynot<const Shape*> shape,
            maynot<const Rendqueue*> rendqueue)
        {
            // RendOb will be input to a chain and used for swap
            m_renderer_list.emplace(
                renderer_arch{
                    rendqueue, trans, mat, shape
                });
        }

        void FlushPipeLine()
        {
            if (glthread)
                if (!jegl_update(glthread))
                {
                    // update is not work now, means graphic thread want to exit..
                    // ready to shutdown current universe

                    if (current_universe)
                        current_universe.stop();
                }
        }
    };

}