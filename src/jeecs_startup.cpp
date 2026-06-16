#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include "wo.h"

#include <list>

void jeal_init();
void je_log_init();
void jegl_shader_generator_init();
void je_extern_lib_woo_api_init();
void je_extern_lib_3rd_pkgs_init();
void je_extern_lib_module_init();

void je_ecs_finish();
void jeal_finish();
void jegl_finish();
void jetowoo_finish();
void je_log_finish();
void jegl_shader_generator_shutdown();
void je_extern_lib_woo_api_finish();
void je_extern_lib_3rd_pkgs_finish();
void je_extern_lib_module_finish();

struct _je_static_context_t
{
    woort_vm* _je_global_panic_hooker = nullptr;
    woort_GCPin* _je_global_panic_hook_function;
    woort_PanicHandlerFunction _je_global_last_panic_handler;

    jegl_graphic_api_entry _jegl_host_graphic_api = nullptr;

    std::mutex _je_delay_free_libs_mx;
    std::list<woort_Dylib*> _je_delay_free_libs;

    jeecs::typing::type_unregister_guard* _je_unregister_guard = nullptr;
};
static _je_static_context_t _je_global_context;

// woort_exe_path() now follows snprintf semantics (fills @p buf, returns the
// path length excluding the NUL terminator, 0 on failure). Wrap it so callers
// can keep using a plain string. The result is cached by the runtime, so this
// is cheap to call repeatedly.
static std::string _je_woort_exe_path()
{
    size_t need = woort_exe_path(nullptr, 0);
    if (need == 0)
        return {};

    std::string path(need, '\0');
    woort_exe_path(path.data(), need + 1);
    return path;
}

void jegl_set_host_graphic_api(jegl_graphic_api_entry api)
{
    _je_global_context._jegl_host_graphic_api = api;
}

jegl_graphic_api_entry jegl_get_host_graphic_api(void)
{
    if (_je_global_context._jegl_host_graphic_api == nullptr)
    {
        return jegl_using_vk120_apis;

#if defined(JE_ENABLE_DX11_GAPI)
        return jegl_using_dx11_apis;
#elif defined(JE_ENABLE_METAL_GAPI)
        return jegl_using_metal_apis;
#elif defined(JE_ENABLE_VK120_GAPI)
        return jegl_using_vk120_apis;
#elif defined(JE_ENABLE_GL330_GAPI) || defined(JE_ENABLE_GLES300_GAPI) || defined(JE_ENABLE_WEBGL20_GAPI)
        return jegl_using_opengl3_apis;
#else
        return jegl_using_none_apis;
#endif
    }
    return _je_global_context._jegl_host_graphic_api;
}

woort_PanicHandler_Action _jedbg_hook_woolang_panic(
    woort_vm* vm,
    const char* functionname,
    const char* src_file,
    int lineno,
    int rterrcode,
    const char* reason)
{
    std::string trace;

    if (vm == nullptr)
        trace = "<No vm running>";
    else
    {
        woort_VMRuntime_TraceCallstack_Iter trace_iter;
        woort_VMRuntime_trace_begin(vm, &trace_iter);

        woort_VMRuntime_TraceCallstack frame;
        while (woort_VMRuntime_trace_next(&trace_iter, &frame))
        {
            const char* const func = frame.m_function_name;
            const char* const file = frame.m_file_or_lib_name;
            const size_t line = frame.m_location_begin[0];
            const size_t col = frame.m_location_begin[1];

            char buf[512];
            if (func != nullptr && file != nullptr)
            {
                if (frame.m_has_location)
                    snprintf(buf, sizeof(buf), "    at %s (%s:%zu:%zu)\n",
                        func, file, line + 1, col + 1);
                else
                    snprintf(buf, sizeof(buf), "    at %s (%s)\n", func, file);
            }
            else if (func != nullptr)
                snprintf(buf, sizeof(buf), "    at %s\n", func);
            else if (file != nullptr)
            {
                if (frame.m_has_location && line != 0)
                    snprintf(buf, sizeof(buf), "    at <unknown> (%s:%zu:%zu)\n",
                        file, line + 1, col + 1);
                else
                    snprintf(buf, sizeof(buf), "    at <unknown> (%s)\n", file);
            }
            else
                snprintf(buf, sizeof(buf), "    at <unknown>\n");

            trace += buf;
        }
    }

    jeecs::debug::logerr("Woolang Panic(%x):%s (%s in %s: %u):\n%s",
        rterrcode, reason, functionname, src_file, lineno, trace.c_str());

    woort_vm* const last_vm = woort_vm_swap(_je_global_context._je_global_panic_hooker);
    {
        woort_value _je_global_panic_hooker_s;
        if (!woort_push_reserve(7, &_je_global_panic_hooker_s))
            // Stack overflow.
            return WOORT_PANIC_HANDLER_ACTION_USE_DEFAULT_HANDLER;

        woort_set_string(_je_global_panic_hooker_s + 0, src_file);
        woort_set_int(_je_global_panic_hooker_s + 1, (woort_Int)lineno);
        woort_set_string(_je_global_panic_hooker_s + 2, functionname);
        woort_set_int(_je_global_panic_hooker_s + 3, (woort_Int)rterrcode);
        woort_set_string(_je_global_panic_hooker_s + 4, reason);
        woort_set_string(_je_global_panic_hooker_s + 5, trace.c_str());

        woort_GCPin_get(
            _je_global_panic_hooker_s + 6,
            _je_global_context._je_global_panic_hook_function,
            0);

        const auto invoke_result =
            woort_invoke(WOORT_IGNORE, _je_global_panic_hooker_s + 6);

        woort_pop(7);

        if (invoke_result != WOORT_VM_CALL_STATUS_NORMAL)
        {
            jeecs::debug::logwarn("Engine's woolang panic hook failed, try default.");
            return WOORT_PANIC_HANDLER_ACTION_USE_DEFAULT_HANDLER;
        }
    }
    (void)woort_vm_swap(last_vm);

    return WOORT_PANIC_HANDLER_ACTION_ABORT;
}

WOORT_API woort_api wojeapi_editor_register_panic_hook(void)
{
    if (_je_global_context._je_global_panic_hooker != nullptr)
        // ATTENTION: Unsafe for multi thread.
        woort_vm_close(_je_global_context._je_global_panic_hooker);

    _je_global_context._je_global_panic_hooker = woort_vm_create();
    if (_je_global_context._je_global_panic_hooker == nullptr)
        return woort_ret_panic("Unable to register panic hook: Failed to create vm.");

    _je_global_context._je_global_panic_hook_function = woort_GCPin_create(1);
    woort_GCPin_set(_je_global_context._je_global_panic_hook_function, 0, 0);

    _je_global_context._je_global_last_panic_handler =
        woort_set_panic_callback(&_jedbg_hook_woolang_panic);

    return woort_ret_void();
}

void je_default_graphic_interface_sync_func(jegl_context* gthread, void*)
{
    std::thread([=]()
        {
            jegl_sync_state state = jegl_sync_state::JEGL_SYNC_SHUTDOWN;
            for (;;)
            {
                jegl_sync_init(gthread, state == jegl_sync_state::JEGL_SYNC_REBOOT);
                do
                {
                    state = jegl_sync_update(gthread);
                } while (state == jegl_sync_state::JEGL_SYNC_COMPLETE);

                if (jegl_sync_shutdown(gthread, state == jegl_sync_state::JEGL_SYNC_REBOOT))
                    break;
            }
        })
        .detach();
}

void _jeecs_entry_register_core_systems(
    jeecs::typing::type_unregister_guard* guard);

void je_init(int argc, char** argv)
{
    assert(_je_global_context._je_delay_free_libs.empty());

    // Update default graphic sync funciton
    jegl_register_sync_thread_callback(
        je_default_graphic_interface_sync_func, nullptr);

    _je_global_context._jegl_host_graphic_api = nullptr;

    je_log_init();

    jegl_shader_generator_init();

    wo_init(argc, argv);

    for (int i = 1; i < argc - 1; ++i)
    {
        std::string config = argv[i];
        std::string value = argv[i + 1];
        if (config == "-gapi")
        {
            if (false)
                ;
#if defined(JE_ENABLE_DX11_GAPI)
            else if (value == "dx11")
                jegl_set_host_graphic_api(jegl_using_dx11_apis);
#endif
#if defined(JE_ENABLE_GL330_GAPI) || defined(JE_ENABLE_GLES300_GAPI) || defined(JE_ENABLE_WEBGL20_GAPI)
            else if (value == "gl3")
                jegl_set_host_graphic_api(jegl_using_opengl3_apis);
#endif
#if defined(JE_ENABLE_VK120_GAPI)
            else if (value == "vk120")
                jegl_set_host_graphic_api(jegl_using_vk120_apis);
#endif
#if defined(JE_ENABLE_METAL_GAPI)
            else if (value == "metal")
                jegl_set_host_graphic_api(jegl_using_metal_apis);
#endif
            else if (value == "none")
                jegl_set_host_graphic_api(jegl_using_none_apis);
            else
                jeecs::debug::logwarn("Unknown graphic api named: '%s'.", value.c_str());
        }
    }

    const auto exe_path = _je_woort_exe_path();
    jeecs_file_set_host_path(exe_path.c_str());
    jeecs_file_set_runtime_path(exe_path.c_str());

    je_extern_lib_woo_api_init();
    je_extern_lib_3rd_pkgs_init();
    je_extern_lib_module_init();

    jeal_init();

    assert(_je_global_context._je_unregister_guard == nullptr);
    _je_global_context._je_unregister_guard = new jeecs::typing::type_unregister_guard();
    jeecs::entry::module_entry(_je_global_context._je_unregister_guard);
    _jeecs_entry_register_core_systems(_je_global_context._je_unregister_guard);
}

uint64_t crc64_of_source_and_api()
{
    uint64_t crc64_result = 0;

    const char* crc64_src = R"(
import woo::std;
import je::internal;

import pkg::fsys;
import pkg::iterator;

using std;
using je::internal;

func main()
{
    let root_dir = fsys::normalize(std::host_path());
    let files = fsys::recursive_walk(root_dir/"builtin")
        |> iterator::iter_result
        |> iterator::filter(\p = fsys::isfile(p) && fsys::extension(p)->lower == ".wo";)
        ;
    let mut crc64_result = "wooscript_crc64_";

    for (let p : files)
    {   
        let path = p->to_string;
        crc64_result += F"{crc64file(path)->or(0)}:{crc64str(path)};";
    }
    return crc64str(crc64_result);
}

return main();
)";

    woort_CodeEnv* const cenv =
        wo_load_source("builtin/je_varify_crc64.wo", crc64_src, nullptr);

    if (cenv != nullptr)
    {
        woort_vm* const vmm = woort_vm_create();
        if (vmm != nullptr)
        {
            woort_vm* const last = woort_vm_swap(vmm);
            {
                woort_value s;
                if (!woort_push_reserve(1, &s))
                    woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow.");
                else if (woort_bootup_codeenv(s, cenv) == WOORT_VM_CALL_STATUS_NORMAL)
                {
                    crc64_result = static_cast<uint64_t>(woort_int(s));
                }
            }
            (void)woort_vm_swap(last);

            woort_vm_close(vmm);
        }
        woort_codeenv_drop(cenv);
    }

    if (crc64_result == 0)
        jeecs::debug::logerr("Unable to eval crc64 of builtin editor scripts.");

    return crc64_result;
}

woort_CodeEnv* _jewo_open_file_to_compile_vm(const char* vpath)
{
    auto* src_file_handle = jeecs_file_open(vpath);
    if (src_file_handle == nullptr)
        return nullptr;

    std::vector<char> src_buffer(src_file_handle->m_file_length);
    jeecs_file_read(src_buffer.data(), sizeof(char), src_file_handle->m_file_length, src_file_handle);
    jeecs_file_close(src_file_handle);

    wo_CompileErrors* cerror;
    woort_CodeEnv* const cenv =
        wo_load_binary(vpath, src_buffer.data(), src_buffer.size(), &cerror);

    if (cenv != nullptr)
        return cenv;

    jeecs::debug::logwarn("Failed to load & create woolang source '%s':\n%s",
        vpath,
        wo_get_compile_error(cerror, WO_COLORFUL));

    wo_compile_errors_free(cerror);
    return nullptr;
}

woort_CodeEnv* try_open_cached_binary()
{
    uint64_t expect_crc = 0;
    auto* srccrc = jeecs_file_open("@/builtin/editor.crc.je4cache");
    if (srccrc == nullptr)
        return nullptr;

    size_t readcount = jeecs_file_read(&expect_crc, sizeof(expect_crc), 1, srccrc);
    jeecs_file_close(srccrc);

    if (readcount < 1)
        return nullptr;

    if (crc64_of_source_and_api() != expect_crc)
        return nullptr;

    return _jewo_open_file_to_compile_vm("@/builtin/editor.woo.je4cache");
}

bool je_main_script_entry()
{
    bool failed_in_start_script = false;

    woort_CodeEnv* cenv = nullptr;
    if ((cenv = _jewo_open_file_to_compile_vm("@/builtin/main.wo")) != nullptr)
    {
        // Load normal entry.
    }
    else if ((cenv = try_open_cached_binary()) != nullptr)
    {
        // Cache loaded, skip,
    }
    else if ((cenv = _jewo_open_file_to_compile_vm(
        (std::string(jeecs_file_get_host_path()) + "/builtin/editor/main.wo").c_str())) != nullptr)
    {
        size_t binary_length;
        void* buffer;

        if (woort_CodeEnv_save_binary(cenv, &buffer, &binary_length))
        {
            FILE* objdump = fopen((_je_woort_exe_path() + "/builtin/editor.woo.je4cache").c_str(), "wb");
            if (objdump != nullptr)
            {
                size_t writelen = fwrite(buffer, 1, binary_length, objdump);
                assert(writelen == binary_length);
                (void)writelen;

                fclose(objdump);
            }
            auto api_src_crc64 = crc64_of_source_and_api();
            FILE* srccrc = fopen((_je_woort_exe_path() + "/builtin/editor.crc.je4cache").c_str(), "wb");
            if (srccrc != nullptr)
            {
                size_t writecount = fwrite(&api_src_crc64, sizeof(api_src_crc64), 1, srccrc);
                assert(writecount == 1);
                (void)writecount;

                fclose(srccrc);
            }
            woort_free(buffer);
        }
    }
    else
    {
        failed_in_start_script = true;
    }

    if (failed_in_start_script == false)
    {
        woort_vm* const vmm = woort_vm_create();
        if (vmm == nullptr)
            jeecs::debug::logerr("Failed to create vm instance.");
        else
        {
            woort_vm* const last = woort_vm_swap(vmm);
            {
                (void)woort_bootup_codeenv(WOORT_IGNORE, cenv);
            }
            (void)woort_vm_swap(last);

            woort_vm_close(vmm);
        }

        woort_codeenv_drop(cenv);
    }

    return !failed_in_start_script;
}

void je_finish()
{
    assert(_je_global_context._je_unregister_guard != nullptr);
    jeecs::entry::module_leave(_je_global_context._je_unregister_guard);

    je_ecs_finish();
    jeal_finish();
    jegl_finish();
    jetowoo_finish();

    if (_je_global_context._je_global_panic_hooker != nullptr)
    {
        woort_vm_close(_je_global_context._je_global_panic_hooker);

        const bool entry_tmp_gc_guard = woort_GC_sync_marking_lock();
        {
            woort_GCPin_destroy(_je_global_context._je_global_panic_hook_function);
        }
        if (entry_tmp_gc_guard)
            woort_GC_sync_marking_unlock();
        
        _je_global_context._je_global_panic_hooker = nullptr;
        _je_global_context._je_global_panic_hook_function = nullptr;

        (void)woort_set_panic_callback(_je_global_context._je_global_last_panic_handler);
        _je_global_context._je_global_last_panic_handler = nullptr;
    }

    (void)woort_set_panic_callback(NULL);

    wo_finish([](void*)
        {
            for (auto* mod : _je_global_context._je_delay_free_libs)
                woort_dylib_unload(mod, WOORT_DYLIB_UNREF);

            _je_global_context._je_delay_free_libs.clear();

            // Free registered external libraries.
            je_extern_lib_module_finish();
            je_extern_lib_3rd_pkgs_finish();
            je_extern_lib_woo_api_finish();
        },
        nullptr);

    delete _je_global_context._je_unregister_guard;
    _je_global_context._je_unregister_guard = nullptr;

    jeecs_file_update_default_fimg(nullptr);
    jegl_shader_generator_shutdown();
    je_log_finish();
}

const char* je_build_version()
{
#define JE_VERSION_WRAP(A, B, C) #A "." #B "." #C
    return "JoyEngine " JE_CORE_VERSION " " __TIMESTAMP__;
#undef JE_VERSION_WRAP
}

const char* je_build_commit()
{
    return
#if __has_include("jeecs_commit_sha.hpp")
#   include "jeecs_commit_sha.hpp"
#else
        "untracked"
#endif
        ;
}

woort_Dylib* je_module_load(const char* name, const char* path)
{
    if (woort_Dylib* lib = woort_dylib_load(name, path, nullptr, false))
    {
        if (auto entry = (jeecs::typing::module_entry_t)
            woort_dylib_load_func(lib, "jeecs_module_entry"))
            entry(lib);

        jeecs::debug::loginfo("Module: '%s'(%p) loaded", path, lib);
        return lib;
    }
    jeecs::debug::logerr("Failed to load module: '%s'", path);
    return nullptr;
}

void* je_module_func(woort_Dylib* lib, const char* funcname)
{
    assert(lib);
    return woort_dylib_load_func(lib, funcname);
}

void je_module_unload(woort_Dylib* lib)
{
    assert(lib);
    if (auto leave = (jeecs::typing::module_leave_t)woort_dylib_load_func(lib, "jeecs_module_leave"))
        leave();
    jeecs::debug::loginfo("Module: '%p' request to unload.", lib);
    woort_dylib_unload(lib, WOORT_DYLIB_BURY);

    // NOTE: Woolang GCptr may invoke some function defined in lib in GC Thread job,
    //  to make sure safety, all the lib will be free in je_finish.
    std::lock_guard g(_je_global_context._je_delay_free_libs_mx);
    _je_global_context._je_delay_free_libs.push_back(lib);
}
