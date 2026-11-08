#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#ifndef JE_ENABLE_DEBUG_API
#   define JE_ENABLE_DEBUG_API
#endif
#include "jeecs.hpp"

namespace jeecs::frame_animation
{
    struct FrameAnimationSystem : public game_system
    {
        double _fixed_time = 0.;

        FrameAnimationSystem(game_world w);
        void StateUpdate();
    };
}
