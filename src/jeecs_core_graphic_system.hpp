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

namespace jeecs
{
    struct DefaultGraphicPipelineSystem : public game_system
    {
        using Translation = Transform::Translation;
        using InverseTranslation = Transform::InverseTranslation;

        using OrthoCamera = Renderer::OrthoCamera;
        using Material = Renderer::Material;
        using Shape = Renderer::Shape;

        jegl_thread* glthread = nullptr;
        game_universe current_universe = nullptr;
        jegl_resource* shader;
        jegl_resource* texture;

        DefaultGraphicPipelineSystem(game_universe universe)
            : game_system(nullptr)
            , current_universe(universe)
        {
            // GraphicSystem is a public system and not belong to any world.
            texture = jegl_load_texture((rs_exe_path() + std::string("funny.png")).c_str());
            shader = jegl_load_shader_source("je/builtin/unlit.shader", R"(
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
    return fragment_out(texture(main_texture, uv));
}

)");

            jegl_interface_config config = {};
            config.m_fps = 60;
            config.m_resolution_x = 320;
            config.m_resolution_y = 240;
            config.m_title = "JoyEngineECS(JoyEngine 4.0)";

            glthread = jegl_start_graphic_thread(
                config,
                jegl_using_opengl_apis,
                [](void* ptr, jegl_thread* glthread)
                {((DefaultGraphicPipelineSystem*)ptr)->Frame(glthread); }, this);

            register_system_func(&DefaultGraphicPipelineSystem::SimplePrepareCamera,
                {
                    contain<InverseTranslation>(),  // Used for inverse mats
                    before(&DefaultGraphicPipelineSystem::FlushPipeLine)
                });
            register_system_func(&DefaultGraphicPipelineSystem::SimpleRendObject,
                {
                    before(&DefaultGraphicPipelineSystem::FlushPipeLine)
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
            const Translation* translation;
            const OrthoCamera* camera;
        };
        struct RendererArch
        {
            const Translation* translation;
            const Material* material;
            const Shape* shape;
        };

        std::vector<CameraArch> m_camera_list;
        std::vector<RendererArch> m_renderer_list;

        void Frame(jegl_thread* glthread)
        {
            // Here to rend a frame..
            float databuf[] = { -0.5f, -0.5f, 0.0f,     0.0f,   0.0f,
                                0.5f, -0.5f, 0.0f,      1.0f,   0.0f,
                                0.5f, 0.5f, 0.0f,       1.0f,   1.0f,
                                -0.5f, 0.5f, 0.0f,      0.0f,  1.0f, };
            size_t vao[] = { 3, 2, 0 };

            auto triangle = jegl_create_vertex(jegl_vertex::QUADS, databuf, vao, 4);

            jegl_using_texture(texture, 0);
            jegl_draw_vertex_with_shader(triangle, shader);
            jegl_close_resource(triangle);

            m_renderer_list.clear();
            m_camera_list.clear();
        }

        void SimplePrepareCamera(const Translation* trans, const OrthoCamera* camera)
        {
            // Calc camera proj matrix
            m_camera_list.push_back(
                CameraArch{
                    trans, camera
                }
            );
        }

        void SimpleRendObject(const Translation* trans, const Material* mat, const Shape* shape)
        {
            // RendOb will be input to a chain and used for swap
            m_renderer_list.emplace_back(
                RendererArch{
                    trans, mat, shape
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