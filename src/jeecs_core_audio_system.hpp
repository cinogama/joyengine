#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#ifndef JE_ENABLE_DEBUG_API
#   define JE_ENABLE_DEBUG_API
#endif
#include "jeecs.hpp"

namespace jeecs::audio
{
    struct AudioUpdatingSystem : game_system
    {
        AudioUpdatingSystem(game_world w);
        void CommitUpdate();
    };
}
