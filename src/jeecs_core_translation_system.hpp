#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs.hpp"

namespace jeecs::translation
{
    struct TranslationUpdatingSystem : public game_system
    {
        TranslationUpdatingSystem(game_world world);

        void TransformStageUpdate();
        void UserInterfaceStageUpdate();

        void TransformUpdate();
        void CommitUpdate();
    };
}
