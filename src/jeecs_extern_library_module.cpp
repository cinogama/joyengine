#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"
#include "jeecs_basic_bootup.hpp"

#ifdef JE_HAVE_EXTERN_LIB_MODULE
#   error Cannot define JE_HAVE_EXTERN_LIB_MODULE manually.
#endif

#if __has_include("jeecs_extern_library_module.hpp")
#   include "jeecs_extern_library_module.hpp"
#   define JE_HAVE_EXTERN_LIB_MODULE
#endif

#include <vector>

namespace jeecs::external_modules {}

using namespace std;
using namespace jeecs;
using namespace jeecs::external_modules;

namespace jeecs::external_modules
{
    namespace
    {
        vector<woort_Dylib*> static_module_lib_handles;
    }

    void bootup()
    {
        assert(static_module_lib_handles.empty());
#ifdef JE_HAVE_EXTERN_LIB_MODULE
        je_extern_lib_module_init_impl(&static_module_lib_handles);
#endif
    }
    void shutdown()
    {
#ifdef JE_HAVE_EXTERN_LIB_MODULE
        je_extern_lib_module_finish_impl(&static_module_lib_handles);
#endif
        assert(static_module_lib_handles.empty());
    }
}
