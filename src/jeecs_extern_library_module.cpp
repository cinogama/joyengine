// THIS FILE IS AUTO GENERATED BY /script/build.wo

#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#if JE4_STATIC_LINK_MODULE_AND_PKGS
extern "C"
{
JE_EXPORT void jestatic_module_core_entry();
JE_EXPORT void jestatic_module_core_leave();

}
#endif

std::vector<void*> _je_static_module_lib_handle;
void je_extern_lib_module_init()
{
    assert(_je_static_module_lib_handle.empty());
#if JE4_STATIC_LINK_MODULE_AND_PKGS
    wo_extern_lib_func_t core_fs[] = {
        wo_extern_lib_func_t{"jeecs_module_entry", (void*)&jestatic_module_core_entry},
        wo_extern_lib_func_t{"jeecs_module_leave", (void*)&jestatic_module_core_leave},
        WO_EXTERN_LIB_FUNC_END,
    };
    _je_static_module_lib_handle.push_back(
        wo_register_lib("core", core_fs));


#endif
}

void je_extern_lib_module_finish()
{
    for (auto* lib : _je_static_module_lib_handle)
    {
        assert(lib != nullptr);
        wo_unload_lib(lib);
    }
    _je_static_module_lib_handle.clear();
}
