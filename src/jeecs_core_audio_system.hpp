#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/game.hpp"

namespace jeecs::audio
{
    struct AudioUpdatingSystem : game_system
    {
        AudioUpdatingSystem(game_world w);
        void CommitUpdate();
    };
}
