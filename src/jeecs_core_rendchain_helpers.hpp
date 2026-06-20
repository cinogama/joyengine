#pragma once

// Private internal helper shared by the core system headers
// (jeecs_core_graphic_system.hpp, jeecs_core_editor_system.hpp, ...).
// Must be included AFTER "jeecs.hpp" so that graphic::INVALID_UNIFORM_LOCATION
// and the jegl_rchain_set_uniform_* C API are visible.

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif

#include "jeecs.hpp"

namespace jeecs
{
    // Sets a builtin uniform on a rendchain draw action only when the shader
    // actually exposes it (location != INVALID_UNIFORM_LOCATION). Avoids one
    // null-check boilerplate per uniform binding site.
#define JE_CHECK_NEED_AND_SET_UNIFORM(ACTION, UNIFORM, ITEM, TYPE, ...)                                  \
    do                                                                                                   \
    {                                                                                                    \
        if (UNIFORM->m_builtin_uniform_##ITEM != graphic::INVALID_UNIFORM_LOCATION)                     \
            jegl_rchain_set_uniform_##TYPE(ACTION, &UNIFORM->m_builtin_uniform_##ITEM, __VA_ARGS__);     \
    } while (0)
}
