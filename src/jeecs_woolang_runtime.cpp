#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"
#include "jeecs_basic_bootup.hpp"

#include "wo.h"
#include "woort.h"

namespace jeecs::woolang_runtime {}

using namespace std;
using namespace jeecs;
using namespace jeecs::woolang_runtime;

namespace jeecs::woolang_runtime
{
    void bootup(int argc, char** argv)
    {
        const wo_WooDyn_Functions funcs = {
            &woort_init,
            &woort_shutdown,
            &woort_dylib_load,
            &woort_dylib_load_func,
            &woort_dylib_unload,
        };
        wo_init(argc, argv, &funcs);
    }
    void shutdown()
    {
        wo_finish(nullptr, nullptr);
    }
}