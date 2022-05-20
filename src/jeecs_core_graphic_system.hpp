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

        DefaultGraphicPipelineSystem(game_universe universe)
            : game_system(nullptr)
            , current_universe(universe)
        {
            // GraphicSystem is a public system and not belong to any world.

            jegl_interface_config config = {};
            config.m_fps = 60;
            config.m_resolution_x = 320;
            config.m_resolution_y = 240;
            config.m_title = "JoyEngineECS(JoyEngine 4.0)";

            glthread = jegl_start_graphic_thread(
                config,
                jegl_using_opengl_apis,
                [](void* ptr){((DefaultGraphicPipelineSystem*)ptr)->Frame();}, this);

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

        void Frame()
        {
            // Here to rend a frame..


            m_renderer_list.clear();
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