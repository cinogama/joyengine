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
        // std::priority_queue 

        DefaultGraphicPipelineSystem()
            : game_system(nullptr)
        {
            // GraphicSystem is a public system and not belong to any world.

            jegl_interface_config config;
            config.m_fps = 60;
            config.m_resolution_x = 1024;
            config.m_resolution_y = 768;

            glthread = jegl_start_graphic_thread(
                config,
                jegl_using_opengl_apis,
                [](void* ptr)
                {
                    ((DefaultGraphicPipelineSystem*)ptr)->Frame();
                }, this);

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

        void Frame()
        {
            // Here to rend a frame..
        }

        void SimplePrepareCamera(const Translation* trans, const OrthoCamera* camera)
        {
            // Camera must contain:
            //  InverseTranslation
        }

        void SimpleRendObject(const Translation* trans, const Material* mat, const Shape* shape)
        {
            // RendOb will be input to a chain and used for swap
        }

        void FlushPipeLine()
        {
            if (glthread)
                jegl_update(glthread);
        }
    };

}