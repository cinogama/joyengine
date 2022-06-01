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
        using InverseTranslation = Transform::InverseTranslation;

        using Rendqueue = Renderer::Rendqueue;
        using OrthoCamera = Renderer::OrthoCamera;
        using Viewport = Renderer::Viewport;
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
                    { -0.5f, -0.5f, 0.0f,     0.0f, 0.0f,
                    0.5f, -0.5f, 0.0f,      1.0f, 0.0f,
                    0.5f, 0.5f, 0.0f,       1.0f, 1.0f,
                    -0.5f, 0.5f, 0.0f,      0.0f, 1.0f, },
                    { 3, 2 });

            default_shader = new graphic::shader("je/default.shader", R"(
// Default shader
import je.shader;

func vert(var vdata:vertex_in)
{
    var ipos = vdata->in:<float3>(0);

    var opos = je_mvp * float4(ipos, 1);

    return vertex_out(opos);
}
func frag(var fdata:fragment_in)
{
    var flashing_color = je_time->y();
    return vertex_out(float4(flashing_color, 0, flashing_color, 1));
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

            register_system_func(&DefaultGraphicPipelineSystem::SimplePrepareCamera,
                {
                    before(&DefaultGraphicPipelineSystem::FlushPipeLine),

                    contain<InverseTranslation>(),  // Used for inverse mats
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

        struct CameraArch
        {
            const Rendqueue* rendqueue;
            const Translation* translation;
            const OrthoCamera* camera;
            const Viewport* viewport;

            bool operator < (const CameraArch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };
        struct RendererArch
        {
            const Rendqueue* rendqueue;
            const Translation* translation;
            const Material* material;
            const Shape* shape;

            bool operator < (const RendererArch& another) const noexcept
            {
                int a_queue = rendqueue ? rendqueue->rend_queue : 0;
                int b_queue = another.rendqueue ? another.rendqueue->rend_queue : 0;
                return a_queue < b_queue;
            }
        };

        std::priority_queue<CameraArch> m_camera_list;
        std::priority_queue<RendererArch> m_renderer_list;

        void Frame(jegl_thread* glthread)
        {
            std::list<RendererArch> m_renderer_entities;

            while (!m_renderer_list.empty())
            {
                m_renderer_entities.push_back(m_renderer_list.top());
                m_renderer_list.pop();
            }

            size_t RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT;
            jegl_get_windows_size(&RENDAIMBUFFER_WIDTH, &RENDAIMBUFFER_HEIGHT);

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
                    if (current_camera.viewport)
                        jegl_rend_to_framebuffer(nullptr,
                            current_camera.viewport->viewport.x,
                            current_camera.viewport->viewport.y,
                            current_camera.viewport->viewport.z,
                            current_camera.viewport->viewport.w);
                    else
                        jegl_rend_to_framebuffer(nullptr, 0, 0, RENDAIMBUFFER_WIDTH, RENDAIMBUFFER_HEIGHT);

                    const float(&MAT4_VIEW)[4][4] = current_camera.translation->object2world;

                    float MAT4_PROJECTION[4][4] = {};

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

        void SimplePrepareCamera(
            const Translation* trans,
            const OrthoCamera* camera,
            maynot<const Rendqueue*> rendqueue,
            maynot<const Viewport*> cameraviewport)
        {
            // Calc camera proj matrix
            m_camera_list.emplace(
                CameraArch{
                    rendqueue, trans, camera, cameraviewport
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
                RendererArch{
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