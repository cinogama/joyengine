#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

const char* commit_sha_from_cicd =
{
#include "jeecs_commit_msg.hpp"
};

extern const char* shader_wrapper_path;
extern const char* shader_wrapper_src;

extern const char* shader_light2d_path;
extern const char* shader_light2d_src;

extern const char* shader_pbr_path;
extern const char* shader_pbr_src;

extern const char* gui_api_path;
extern const char* gui_api_src;

extern const char* jeecs_woolang_api_path;
extern const char* jeecs_woolang_api_src;

extern const char* jeecs_woolang_editor_api_path;
extern const char* jeecs_woolang_editor_api_src;

extern const char* jeecs_towoo_system_path;
extern const char* jeecs_towoo_system_src;

extern const char* jeecs_towoo_component_path;
extern const char* jeecs_towoo_component_src;

extern const char* jeecs_towoo_path;
extern const char* jeecs_towoo_src;

extern const char* jeecs_physics2d_config_path;
extern const char* jeecs_physics2d_config_src;

void jeal_init();
void je_log_init();
void jegl_shader_generator_init();

void je_ecs_finish();
void jeal_finish();
void jegl_finish();
void jetowoo_finish();
void je_log_finish();
void jegl_shader_generator_shutdown();

void jewo_clear_global_pin_value();

wo_fail_handler _je_global_old_panic_handler = nullptr;
wo_vm _je_global_panic_hooker = nullptr;
wo_value _je_global_panic_hook_function;

jegl_graphic_api_entry _jegl_host_graphic_api = nullptr;

jeecs::typing::type_unregister_guard* _je_unregister_guard = nullptr;

void jegl_set_host_graphic_api(jegl_graphic_api_entry api)
{
    _jegl_host_graphic_api = api;
}

jegl_graphic_api_entry jegl_get_host_graphic_api(void)
{
    return jegl_using_vulkan110_apis;
    if (_jegl_host_graphic_api == nullptr)
    {
#if defined(JE_ENABLE_DX11_GAPI)
        return jegl_using_dx11_apis;
#elif defined(JE_ENABLE_GL330_GAPI) || defined(JE_ENABLE_GLES300_GAPI)
        return jegl_using_opengl3_apis;
#elif defined(JE_ENABLE_VK110_GAPI)
        return jegl_using_vulkan110_apis;
#elif defined(JE_ENABLE_METAL_GAPI)
        return jegl_using_metal_apis;
#else
        jeecs::debug::logwarn("No default graphic-api supported.");
        return jegl_using_none_apis;
#endif
    }
    return _jegl_host_graphic_api;
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

    wo_push_string(_je_global_panic_hooker, trace == nullptr ? "<no-found>" : trace);
    wo_push_string(_je_global_panic_hooker, reason);
    wo_push_int(_je_global_panic_hooker, (wo_integer_t)rterrcode);
    wo_push_string(_je_global_panic_hooker, functionname);
    wo_push_int(_je_global_panic_hooker, (wo_integer_t)lineno);
    wo_push_string(_je_global_panic_hooker, src_file);

    if (wo_invoke_value(_je_global_panic_hooker, _je_global_panic_hook_function, 6) != nullptr)
    {
        // Abort specify vm;
        wo_abort_vm(vm);
    }
    else
    {
        jeecs::debug::logwarn("Engine's woolang panic hook failed, try default.");
        assert(_je_global_old_panic_handler != nullptr);
        _je_global_old_panic_handler(vm, src_file, lineno, functionname, rterrcode, reason);
    }
}

WO_API wo_api wojeapi_editor_register_panic_hook(wo_vm vm, wo_value args, size_t argc)
{
    if (_je_global_panic_hooker != nullptr)
        // ATTENTION: Unsafe for multi thread.
        wo_release_vm(_je_global_panic_hooker);

    _je_global_panic_hooker = wo_borrow_vm(vm);
    _je_global_panic_hook_function = wo_push_val(_je_global_panic_hooker, args + 0);

    if (_je_global_old_panic_handler == nullptr)
        _je_global_old_panic_handler = wo_regist_fail_handler(_jedbg_hook_woolang_panic);

    return wo_ret_void(vm);
}

void je_default_graphic_interface_sync_func(jegl_thread* gthread, void*)
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

        }).detach();
}

void je_init(int argc, char** argv)
{
    // Update default graphic sync funciton
    jegl_register_sync_thread_callback(
        je_default_graphic_interface_sync_func, nullptr);

    _jegl_host_graphic_api = nullptr;

    je_log_init();

    jegl_shader_generator_init();

    wo_init(argc, argv);

    for (int i = 1; i < argc - 1; ++i)
    {
        std::string config = argv[i];
        std::string value = argv[i + 1];
        if (config == "-gapi")
        {
            if (false);
#if defined(JE_ENABLE_DX11_GAPI)
            else if (value == "dx11")
                jegl_set_host_graphic_api(jegl_using_dx11_apis);
#endif
#if defined(JE_ENABLE_GL330_GAPI)|| defined(JE_ENABLE_GLES300_GAPI)
            else if (value == "gl3")
                jegl_set_host_graphic_api(jegl_using_opengl3_apis);
#endif
#if defined(JE_ENABLE_VK110_GAPI)
            else if (value == "vk110")
                jegl_set_host_graphic_api(jegl_using_vulkan110_apis);
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

    wo_virtual_source(jeecs_physics2d_config_path, jeecs_physics2d_config_src, false);
    wo_virtual_source(jeecs_towoo_path, jeecs_towoo_src, false);
    wo_virtual_source(jeecs_towoo_component_path, jeecs_towoo_component_src, false);
    wo_virtual_source(jeecs_towoo_system_path, jeecs_towoo_system_src, false);
    wo_virtual_source(jeecs_woolang_editor_api_path, jeecs_woolang_editor_api_src, false);
    wo_virtual_source(jeecs_woolang_api_path, jeecs_woolang_api_src, false);
    wo_virtual_source(shader_wrapper_path, shader_wrapper_src, false);
    wo_virtual_source(shader_light2d_path, shader_light2d_src, false);
    wo_virtual_source(shader_pbr_path, shader_pbr_src, false);
    wo_virtual_source(gui_api_path, gui_api_src, false);
    jeal_init();

    _je_unregister_guard = new jeecs::typing::type_unregister_guard();
    jeecs::entry::module_entry(_je_unregister_guard);
}

wo_integer_t crc64_of_source_and_api()
{
    wo_integer_t crc64_result = 0;

    const char* crc64_src = R"(
import woo::std;
import pkg::fsys;
import je::editor;

using std;
using je::editor;

let root_dir = fsys::normalize(std::exepath());
let files = fsys::allsubpath(root_dir/"builtin/api")
    ->  unwarpor([])
    ->  connect(fsys::allsubpath(root_dir/"builtin/editor")
            ->unwarpor([]));

let mut crc64_result = "wooscript_crc64_";

for (let _, p : files)
{   
    let path = p->tostring;
    crc64_result += F"{crc64file(path)->valor(0)}:{crc64str(path)};";
}

return crc64str(crc64_result);
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
        jeecs::debug::logwarn("Unable to eval crc64 of builtin editor scripts.");

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
    auto* srccrc = jeecs_file_open("@/builtin/editor.crc.jecache4");
    if (srccrc == nullptr)
        return nullptr;

    size_t readcount = jeecs_file_read(&expect_crc, sizeof(expect_crc), 1, srccrc);
    jeecs_file_close(srccrc);

    if (readcount < 1)
        return nullptr;

    if (crc64_of_source_and_api() != expect_crc)
        return nullptr;

    return _jewo_open_file_to_compile_vm("@/builtin/editor.woo.jecache4");
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
    else if ((vmm = _jewo_open_file_to_compile_vm("@/builtin/editor/main.wo")) != nullptr)
    {
        size_t binary_length;
        void* buffer = wo_dump_binary(vmm, true, &binary_length);

        FILE* objdump = fopen((std::string(wo_exe_path()) + "/builtin/editor.woo.jecache4").c_str(), "wb");
        if (objdump != nullptr)
        {
            size_t writelen = fwrite(buffer, 1, binary_length, objdump);
            assert(writelen == binary_length);
            fclose(objdump);
        }
        auto api_src_crc64 = crc64_of_source_and_api();
        FILE* srccrc = fopen((std::string(wo_exe_path()) + "/builtin/editor.crc.jecache4").c_str(), "wb");
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

std::vector<void*> _free_module_list;
std::mutex _free_module_list_mx;

void je_finish()
{
    jeecs::entry::module_leave(_je_unregister_guard);

    je_ecs_finish();
    jeal_finish();
    jegl_finish();
    jetowoo_finish();

    if (_je_global_panic_hooker != nullptr)
    {
        wo_release_vm(_je_global_panic_hooker);
        _je_global_panic_hooker = nullptr;
    }
    if (_je_global_old_panic_handler != nullptr)
    {
        wo_regist_fail_handler(_je_global_old_panic_handler);
        _je_global_old_panic_handler = nullptr;
    }

    jewo_clear_global_pin_value();

    wo_finish([](void*)
        {
            std::lock_guard g1(_free_module_list_mx);
            for (auto* mod : _free_module_list)
                wo_unload_lib(mod);

            _free_module_list.clear();
        }, nullptr
    );

    delete _je_unregister_guard;
    _je_unregister_guard = nullptr;

    jeecs_file_update_default_fimg(nullptr);

    jegl_shader_generator_shutdown();

    je_log_finish();

}

const char* je_build_version()
{
    return "JoyEngine 4.3.6 " __TIMESTAMP__;
}

const char* je_build_commit()
{
    return commit_sha_from_cicd;
}

void* je_module_load(const char* name, const char* path)
{
    if (void* lib = wo_load_lib(name, path, false))
    {
        if (auto entry = (jeecs::typing::module_entry_t)
            wo_load_func(lib, "jeecs_module_entry"))
            entry();

        jeecs::debug::loginfo("Module: '%s'(%p) loaded", path, lib);
        return lib;
    }
    jeecs::debug::logerr("Failed to load module: '%s'", path);
    return nullptr;
}

void* je_module_func(void* lib, const char* funcname)
{
    assert(lib);
    return wo_load_func(lib, funcname);
}

void je_module_unload(void* lib)
{
    assert(lib);
    if (auto leave = (jeecs::typing::module_leave_t)
        wo_load_func(lib, "jeecs_module_leave"))
        leave();
    jeecs::debug::loginfo("Module: '%p' request to unloaded", lib);
    wo_unload_lib(lib);
}

void je_module_delay_unload(void* lib)
{
    assert(lib);
    if (auto leave = (jeecs::typing::module_leave_t)
        wo_load_func(lib, "jeecs_module_leave"))
        leave();
    jeecs::debug::loginfo("Module: '%p' request to unloaded", lib);
    std::lock_guard g1(_free_module_list_mx);
    _free_module_list.push_back(lib);
}
