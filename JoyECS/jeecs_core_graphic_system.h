#pragma once

// Important:
//  Graphic is a world-less system, it can be switched from one to another system.
//
// How to switch system?
//  In universe, we can find the system with typeinfo and get current world, a task
//  will be push_front to 'pick-out' system function and re-add it to another world.


#define JE_IMPL
#include "jeecs.hpp"

namespace jeecs
{
    struct DefaultGraphicPipelineSystem : public game_system
    {
        using Translation = Transform::Translation;

        using OrthoCamera = Renderer::OrthoCamera;
        using Material = Renderer::Material;
        using Shape = Renderer::Shape;
        
        DefaultGraphicPipelineSystem()
            : game_system(nullptr)
        {
            // GraphicSystem is a public system and not belong to any world.
        }

        void SimpleRendJob(const Translation* trans, const Material* mat, const Shape* shape)
        {
            
        }
    };

}