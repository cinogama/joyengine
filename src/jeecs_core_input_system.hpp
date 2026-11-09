#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs.hpp"

namespace jeecs::io
{
    struct VirtualGamepadInputSystem : public game_system
    {
        VirtualGamepadInputSystem(game_world world);
        void StateUpdate();
    };
}