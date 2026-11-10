#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/game.hpp"

namespace jeecs::animation
{
    struct AnimatorSystem : public game_system
    {
        AnimatorSystem(game_world w);
        void StateUpdate();
    };
}
