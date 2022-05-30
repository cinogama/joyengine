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
        using Material = Renderer::Material;
        using Shape = Renderer::Shape;

        jegl_thread* glthread = nullptr;
        game_universe current_universe = nullptr;

        jegl_resource* shader;
        jegl_resource* texture;
        jegl_resource* vertex;

        DefaultGraphicPipelineSystem(game_universe universe)
            : game_system(nullptr)
            , current_universe(universe)
        {
            // GraphicSystem is a public system and not belong to any world.
            texture = jegl_load_texture((rs_exe_path() + std::string("funny.png")).c_str());
            float databuf[] = { -1.0f, -1.0f, 0.0f,     0.0f,   0.0f,
                              1.f, -1.0f, 0.0f,      1.0f,   0.0f,
                              1.f, 1.f, 0.0f,       1.0f,   1.0f,
                              -1.f, 1.f, 0.0f,      0.0f,  1.0f, };
            size_t vao[] = { 3, 2, 0 };

            vertex = jegl_create_vertex(jegl_vertex::QUADS, databuf, vao, 4);
            shader = jegl_load_shader_source("je/builtin/unlit.shader", u8R"(
import je.shader;

func vert(var vdata : vertex_in)
{
    var ipos = vdata->in:<float3>(0);
    var iuv = vdata->in:<float2>(1);
    return vertex_out(float4(ipos, 1), iuv);
}

var main_texture = uniform:<texture2d>("MAIN_TEXTURE");

func frag(var fdata : fragment_in)
{
    var uv = fdata->in:<float2>(1);
    return fragment_out(float4(1,0,1,1));
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
            // Here to rend a frame..

            if (texture)
                jegl_using_texture(texture, 0);

            jegl_draw_vertex_with_shader(vertex, shader);

            while (!m_camera_list.empty())
            {
                auto& current_camera = m_camera_list.top();
                {

                }
                m_camera_list.pop();
            }

            while (!m_renderer_list.empty())
                m_renderer_list.pop();

            while (!m_camera_list.empty())
                m_camera_list.pop();
        }

        void SimplePrepareCamera(const Translation* trans, const OrthoCamera* camera, maynot<const Rendqueue*> rendqueue)
        {
            // Calc camera proj matrix
            m_camera_list.emplace(
                CameraArch{
                    rendqueue, trans, camera
                }
            );
        }

        void SimpleRendObject(const Translation* trans, const Material* mat, const Shape* shape, maynot<const Rendqueue*> rendqueue)
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