#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#   define JE_ENABLE_DEBUG_API
#   include "jeecs.hpp"
#endif

#include <box2d/box2d.h>

namespace jeecs
{
    struct Physics2DUpdatingSystem :public game_system
    {
        b2World m_physics_world;

        Physics2DUpdatingSystem(game_world world) :game_system(world)
            , m_physics_world(b2Vec2(0.f, -9.8f))
        {
            m_physics_world.SetAllowSleeping(true);
        }

        void PreUpdate()
        {

        }

        void Update()
        {
        }

        void LateUpdate()
        {
        }
    };
}
