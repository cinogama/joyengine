#ifndef JE_IMPL
#define JE_IMPL
#endif

#include "jeecs.hpp"

// EDITOR API ONLY USED IN WHEN EDIT, DO NOT USE THEM IN RUNTIME
extern "C"
{
    JE_API void jedbg_set_editor_universe(void * universe_handle);
}