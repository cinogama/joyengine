#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

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

void _jewo_clear_singletons();

struct _je_static_context_t
{
    wo_fail_handler_t _je_global_old_panic_handler = nullptr;
    wo_vm _je_global_panic_hooker = nullptr;
    wo_pin_value _je_global_panic_hook_function;

    jegl_graphic_api_entry _jegl_host_graphic_api = nullptr;

    std::mutex _je_delay_free_libs_mx;
    std::list<void*> _je_delay_free_libs;

    jeecs::typing::type_unregister_guard* _je_unregister_guard = nullptr;
};
_je_static_context_t _je_global_context;


void jegl_set_host_graphic_api(jegl_graphic_api_entry api)
{
    _je_global_context._jegl_host_graphic_api = api;
}

jegl_graphic_api_entry jegl_get_host_graphic_api(void)
{
    if (_je_global_context._jegl_host_graphic_api == nullptr)
    {
#if defined(JE_ENABLE_DX11_GAPI)
        return jegl_using_dx11_apis;
#elif defined(JE_ENABLE_GL330_GAPI) || defined(JE_ENABLE_GLES300_GAPI) || defined(JE_ENABLE_WEBGL20_GAPI)
        return jegl_using_opengl3_apis;
#elif defined(JE_ENABLE_VK130_GAPI)
        return jegl_using_vk130_apis;
#elif defined(JE_ENABLE_METAL_GAPI)
        return jegl_using_metal_apis;
#else
        jeecs::debug::logwarn("No default graphic-api supported.");
        return jegl_using_none_apis;
#endif
    }
    return _je_global_context._jegl_host_graphic_api;
}

void _jedbg_hook_woolang_panic(
    wo_vm vm,
    wo_string_t src_file,
    uint32_t lineno,
    wo_string_t functionname,
    uint32_t rterrcode,
    wo_string_t reason)
{
    auto* trace = vm == nullptr ? nullptr : wo_debug_trace_callstack(vm, 32);
    jeecs::debug::logerr("Woolang Panic(%x):%s (%s in %s: %u):\n%s",
        rterrcode, reason, functionname, src_file, lineno,
        trace == nullptr ? "<no-found>" : trace);

    wo_value _je_global_panic_hooker_s =
        wo_reserve_stack(_je_global_context._je_global_panic_hooker, 7, nullptr);

    wo_set_string(_je_global_panic_hooker_s + 0, _je_global_context._je_global_panic_hooker, src_file);
    wo_set_int(_je_global_panic_hooker_s + 1, (wo_integer_t)lineno);
    wo_set_string(_je_global_panic_hooker_s + 2, _je_global_context._je_global_panic_hooker, functionname);
    wo_set_int(_je_global_panic_hooker_s + 3, (wo_integer_t)rterrcode);
    wo_set_string(_je_global_panic_hooker_s + 4, _je_global_context._je_global_panic_hooker, reason);
    wo_set_string(_je_global_panic_hooker_s + 5, _je_global_context._je_global_panic_hooker, trace == nullptr ? "<no-found>" : trace);

    wo_pin_value_get(_je_global_panic_hooker_s + 6, _je_global_context._je_global_panic_hook_function);

    if (nullptr != wo_invoke_value(
        _je_global_context._je_global_panic_hooker,
        _je_global_panic_hooker_s + 6,
        6,
        nullptr,
        &_je_global_panic_hooker_s))
    {
        // Abort specify vm;
        wo_abort_vm(vm);
    }
    else
    {
        jeecs::debug::logwarn("Engine's woolang panic hook failed, try default.");
        assert(_je_global_context._je_global_old_panic_handler != nullptr);
        _je_global_context._je_global_old_panic_handler(vm, src_file, lineno, functionname, rterrcode, reason);
    }

    wo_pop_stack(_je_global_context._je_global_panic_hooker, 7);
}

WO_API wo_api wojeapi_editor_register_panic_hook(wo_vm vm, wo_value args)
{
    if (_je_global_context._je_global_panic_hooker != nullptr)
        // ATTENTION: Unsafe for multi thread.
        wo_release_vm(_je_global_context._je_global_panic_hooker);

    _je_global_context._je_global_panic_hooker = wo_borrow_vm(vm);
    _je_global_context._je_global_panic_hook_function = wo_create_pin_value();

    wo_pin_value_set(_je_global_context._je_global_panic_hook_function, args + 0);

    if (_je_global_context._je_global_old_panic_handler == nullptr)
        _je_global_context._je_global_old_panic_handler = 
        wo_register_fail_handler(_jedbg_hook_woolang_panic);

    return wo_ret_void(vm);
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
    assert(_je_delay_free_libs.empty());

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
#if defined(JE_ENABLE_VK130_GAPI)
            else if (value == "vk130")
                jegl_set_host_graphic_api(jegl_using_vk130_apis);
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

    jeecs_file_set_host_path(wo_exe_path());
    jeecs_file_set_runtime_path(wo_exe_path());

    je_extern_lib_woo_api_init();
    je_extern_lib_3rd_pkgs_init();
    je_extern_lib_module_init();

    jeal_init();

    assert(_je_unregister_guard == nullptr);
    _je_global_context._je_unregister_guard = new jeecs::typing::type_unregister_guard();
    jeecs::entry::module_entry(_je_global_context._je_unregister_guard);
    _jeecs_entry_register_core_systems(_je_global_context._je_unregister_guard);
}

wo_integer_t crc64_of_source_and_api()
{
    wo_integer_t crc64_result = 0;

    const char* crc64_src = R"(
import woo::std;
import je::internal;
import pkg::fsys;

using std;
using je::internal;

func main()
{
    let root_dir = fsys::normalize(std::host_path());
    let files = fsys::allsubpath(root_dir/"builtin/api")
        ->  or([])
        ->  connect(fsys::allsubpath(root_dir/"builtin/editor")
                ->or([]));

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

    wo_vm vmm = wo_create_vm();
    if (wo_load_source(vmm, "builtin/je_varify_crc64.wo", crc64_src))
    {
        wo_jit(vmm);
        wo_value result = wo_run(vmm);
        if (result != nullptr)
            crc64_result = wo_int(result);
    }

    wo_close_vm(vmm);

    if (crc64_result == 0)
        jeecs::debug::logerr("Unable to eval crc64 of builtin editor scripts.");

    return crc64_result;
}

wo_vm _jewo_open_file_to_compile_vm(const char* vpath)
{
    auto* src_file_handle = jeecs_file_open(vpath);
    if (src_file_handle == nullptr)
        return nullptr;

    std::vector<char> src_buffer(src_file_handle->m_file_length);
    jeecs_file_read(src_buffer.data(), sizeof(char), src_file_handle->m_file_length, src_file_handle);
    jeecs_file_close(src_file_handle);

    wo_vm vmm = wo_create_vm();
    if (wo_load_binary(vmm, vpath, src_buffer.data(), src_buffer.size()))
        return vmm;

    jeecs::debug::logwarn("Failed to load & create woolang source '%s':\n%s",
        vpath,
        wo_get_compile_error(vmm, WO_NEED_COLOR));

    wo_close_vm(vmm);
    return nullptr;
}

wo_vm try_open_cached_binary()
{
    wo_integer_t expect_crc = 0;
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

    wo_vm vmm = nullptr;
    if ((vmm = _jewo_open_file_to_compile_vm("@/builtin/main.wo")) != nullptr)
    {
        // Load normal entry.
    }
    else if ((vmm = try_open_cached_binary()) != nullptr)
    {
        // Cache loaded, skip,
    }
    else if ((vmm = _jewo_open_file_to_compile_vm(
        (std::string(jeecs_file_get_host_path()) + "/builtin/editor/main.wo").c_str())) != nullptr)
    {
        size_t binary_length;
        void* buffer = wo_dump_binary(vmm, true, &binary_length);

        FILE* objdump = fopen((std::string(wo_exe_path()) + "/builtin/editor.woo.je4cache").c_str(), "wb");
        if (objdump != nullptr)
        {
            size_t writelen = fwrite(buffer, 1, binary_length, objdump);
            assert(writelen == binary_length);
            fclose(objdump);
        }
        auto api_src_crc64 = crc64_of_source_and_api();
        FILE* srccrc = fopen((std::string(wo_exe_path()) + "/builtin/editor.crc.je4cache").c_str(), "wb");
        if (srccrc != nullptr)
        {
            size_t writecount = fwrite(&api_src_crc64, sizeof(api_src_crc64), 1, srccrc);
            assert(writecount == 1);
            fclose(srccrc);
        }
        wo_free_binary(buffer);
    }
    else
    {
        failed_in_start_script = true;
    }

    if (failed_in_start_script == false)
    {
        wo_jit(vmm);
        wo_run(vmm);
        wo_close_vm(vmm);
    }

    return !failed_in_start_script;
}

void je_finish()
{
    assert(_je_unregister_guard != nullptr);
    jeecs::entry::module_leave(_je_global_context._je_unregister_guard);

    je_ecs_finish();
    jeal_finish();
    jegl_finish();
    jetowoo_finish();

    if (_je_global_context._je_global_panic_hooker != nullptr)
    {
        wo_release_vm(_je_global_context._je_global_panic_hooker);
        wo_close_pin_value(_je_global_context._je_global_panic_hook_function);

        _je_global_context._je_global_panic_hooker = nullptr;
        _je_global_context._je_global_panic_hook_function = nullptr;
    }
    if (_je_global_context._je_global_old_panic_handler != nullptr)
    {
        wo_register_fail_handler(_je_global_context._je_global_old_panic_handler);
        _je_global_context._je_global_old_panic_handler = nullptr;
    }

    _jewo_clear_singletons();

    wo_finish([](void*)
        {
            for (auto* mod : _je_global_context._je_delay_free_libs)
                wo_unload_lib(mod, WO_DYLIB_UNREF);

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
    return "JoyEngine " JE_CORE_VERSION " " __TIMESTAMP__;
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

wo_dylib_handle_t je_module_load(const char* name, const char* path)
{
    if (wo_dylib_handle_t lib = wo_load_lib(name, path, nullptr, false))
    {
        if (auto entry = (jeecs::typing::module_entry_t)
            wo_load_func(lib, "jeecs_module_entry"))
            entry(lib);

        jeecs::debug::loginfo("Module: '%s'(%p) loaded", path, lib);
        return lib;
    }
    jeecs::debug::logerr("Failed to load module: '%s'", path);
    return nullptr;
}

void* je_module_func(wo_dylib_handle_t lib, const char* funcname)
{
    assert(lib);
    return wo_load_func(lib, funcname);
}

void je_module_unload(wo_dylib_handle_t lib)
{
    assert(lib);
    if (auto leave = (jeecs::typing::module_leave_t)wo_load_func(lib, "jeecs_module_leave"))
        leave();
    jeecs::debug::loginfo("Module: '%p' request to unload.", lib);
    wo_unload_lib(lib, WO_DYLIB_BURY);

    // NOTE: Woolang GCptr may invoke some function defined in lib in GC Thread job,
    //  to make sure safety, all the lib will be free in je_finish.
    std::lock_guard g(_je_global_context._je_delay_free_libs_mx);
    _je_global_context._je_delay_free_libs.push_back(lib);
}
