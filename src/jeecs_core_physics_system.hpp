#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#   define JE_ENABLE_DEBUG_API
#   include "jeecs.hpp"
#endif

namespace jeecs
{
    struct PhysicsUpdatingSystem :public game_system
    {
        PhysicsUpdatingSystem(game_world world) :game_system(world)
        {
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
