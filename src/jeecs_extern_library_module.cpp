#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#ifdef JE_HAVE_EXTERN_LIB_MODULE
#   error Cannot define JE_HAVE_EXTERN_LIB_MODULE manually.
#endif

#if __has_include("jeecs_extern_library_module.hpp")
#   include "jeecs_extern_library_module.hpp"
#   define JE_HAVE_EXTERN_LIB_MODULE
#endif

std::vector<wo_dylib_handle_t> _je_static_module_lib_handle;
void je_extern_lib_module_init()
{
    assert(_je_static_module_lib_handle.empty());
#ifdef JE_HAVE_EXTERN_LIB_MODULE
    je_extern_lib_module_init_impl(&_je_static_module_lib_handle);
#endif
}

void je_extern_lib_module_finish()
{
#ifdef JE_HAVE_EXTERN_LIB_MODULE
    je_extern_lib_module_finish_impl(&_je_static_module_lib_handle);
#endif
    assert(_je_static_module_lib_handle.empty());
}
