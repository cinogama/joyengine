#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"
#include "jeecs_basic_bootup.hpp"

#include "wo.h"

#include <list>

void je_ecs_finish();
void jegl_finish();
void jetowoo_finish();

struct _je_static_context_t
{
    woort_vm* _je_global_panic_hooker = nullptr;
    woort_GCPin* _je_global_panic_hook_function;
    woort_PanicHandlerFunction _je_global_last_panic_handler;

    je_gl_graphic_api_entry_t _jegl_host_graphic_api = nullptr;

    jeecs::typing::type_unregister_guard* _je_unregister_guard = nullptr;
};
static _je_static_context_t _je_global_context;

void je_graphic_host_api_set(/* OPTIONAL: nullptr to use platform default */ je_gl_graphic_api_entry_t api)
{
    _je_global_context._jegl_host_graphic_api = api;
}

je_gl_graphic_api_entry_t je_graphic_host_api_get(void)
{
    if (_je_global_context._jegl_host_graphic_api == nullptr)
    {
#if defined(JE_ENABLE_DX11_GAPI)
        return je_graphic_using_dx11_apis;
#elif defined(JE_ENABLE_METAL_GAPI)
        return je_graphic_using_metal_apis;
#elif defined(JE_ENABLE_VK120_GAPI)
        return je_graphic_using_vk120_apis;
#elif defined(JE_ENABLE_GL330_GAPI) || defined(JE_ENABLE_GLES300_GAPI) || defined(JE_ENABLE_WEBGL20_GAPI)
        return je_graphic_using_opengl3_apis;
#else
        return je_graphic_using_none_apis;
#endif
    }
    return _je_global_context._jegl_host_graphic_api;
}

static woort_PanicHandler_Action _jedbg_hook_woolang_panic(
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

static void je_default_graphic_interface_sync_func(je_gl_context_t* gthread, void*)
{
    std::thread([=]()
        {
            je_gl_sync_state_t state = je_gl_sync_state_t::JE_GL_SYNC_SHUTDOWN;
            for (;;)
            {
                je_graphic_sync_init(gthread, state == je_gl_sync_state_t::JE_GL_SYNC_REBOOT);
                do
                {
                    state = je_graphic_sync_update(gthread);
                } while (state == je_gl_sync_state_t::JE_GL_SYNC_COMPLETE);

                if (je_graphic_sync_shutdown(gthread, state == je_gl_sync_state_t::JE_GL_SYNC_REBOOT))
                    break;
            }
        })
        .detach();
}

void _jeecs_entry_register_core_systems(
    jeecs::typing::type_unregister_guard* guard);

void je_core_init(int argc, /* OPTIONAL: If argc == 0 */ char** argv)
{
    jeecs::bootup(argc, argv);

    // Update default graphic sync funciton
    je_graphic_sync_thread_callback_register(
        je_default_graphic_interface_sync_func, nullptr);

    _je_global_context._jegl_host_graphic_api = nullptr;

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
                je_graphic_host_api_set(je_graphic_using_dx11_apis);
#endif
#if defined(JE_ENABLE_GL330_GAPI) || defined(JE_ENABLE_GLES300_GAPI) || defined(JE_ENABLE_WEBGL20_GAPI)
            else if (value == "gl3")
                je_graphic_host_api_set(je_graphic_using_opengl3_apis);
#endif
#if defined(JE_ENABLE_VK120_GAPI)
            else if (value == "vk120")
                je_graphic_host_api_set(je_graphic_using_vk120_apis);
#endif
#if defined(JE_ENABLE_METAL_GAPI)
            else if (value == "metal")
                je_graphic_host_api_set(je_graphic_using_metal_apis);
#endif
            else if (value == "none")
                je_graphic_host_api_set(je_graphic_using_none_apis);
            else
                jeecs::debug::logwarn("Unknown graphic api named: '%s'.", value.c_str());
        }
    }

    assert(_je_global_context._je_unregister_guard == nullptr);
    _je_global_context._je_unregister_guard = new jeecs::typing::type_unregister_guard();
    jeecs::entry::module_entry(_je_global_context._je_unregister_guard);
    _jeecs_entry_register_core_systems(_je_global_context._je_unregister_guard);
}

static woort_CodeEnv* _je_open_file_to_compile_vm(const char* vpath)
{
    auto* src_file_handle = je_file_open(vpath);
    if (src_file_handle == nullptr)
        return nullptr;

    std::vector<char> src_buffer(je_file_size(src_file_handle));
    je_file_read(src_buffer.data(), sizeof(char), src_buffer.size(), src_file_handle);
    je_file_close(src_file_handle);

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

static woort_CodeEnv* _je_try_open_cached_binary()
{
    return _je_open_file_to_compile_vm("!/builtin/editor.woo.je4cache");
}

bool je_core_main_script_entry()
{
    bool failed_in_start_script = false;

    woort_CodeEnv* cenv = nullptr;
    if ((cenv = _je_open_file_to_compile_vm("@/builtin/main.wo")) != nullptr)
    {
        // Load normal entry.
    }
    else if ((cenv = _je_try_open_cached_binary()) != nullptr)
    {
        // Cache loaded, skip,
    }
    else if ((cenv = _je_open_file_to_compile_vm("!/builtin/editor/main.wo")) != nullptr)
    {
        size_t binary_length;
        void* buffer;

        if (woort_CodeEnv_save_binary(cenv, &buffer, &binary_length))
        {
            auto* const cache = je_cache_file_create("!/builtin/editor/main.wo", 1, 0);
            bool succ = false;
            if (cache != nullptr) do
            {
                if (!je_cache_binary_append_document(cache))
                    break;
                if (!je_cache_binary_append_string(cache, "content"))
                    break;
                if (!je_cache_binary_append_buffer(cache, buffer, binary_length))
                    break;
                if (!je_cache_binary_append_end(cache))
                    break;

                succ = true;
            } while (0);

            if (!succ)
                jeecs::debug::logwarn("Failed to create editor script binary cache.");

            if (cache != nullptr)
                je_cache_file_close(cache, succ);

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
                (void)woort_bootup(WOORT_IGNORE, cenv, true);
            }
            (void)woort_vm_swap(last);

            woort_vm_close(vmm);
        }

        woort_codeenv_drop(cenv);
    }

    return !failed_in_start_script;
}

void je_core_finish()
{
    assert(_je_global_context._je_unregister_guard != nullptr);
    jeecs::entry::module_leave(_je_global_context._je_unregister_guard);

    je_ecs_finish();
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


    jeecs::shutdown();

    delete _je_global_context._je_unregister_guard;
    _je_global_context._je_unregister_guard = nullptr;

    je_fimg_default_archive_set(nullptr);
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

/* OPTIONAL: If load failed */ woort_Dylib* je_module_load(const char* name, const char* path)
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

/* OPTIONAL: If symbol not found */ void* je_module_func(woort_Dylib* lib, const char* funcname)
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
    woort_dylib_unload(lib, WOORT_DYLIB_UNREF_AND_BURY);
}
