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

void je_ecs_shutdown();
void jeal_init();
void jeal_finish();

void jegl_finish();

void jetowoo_finish();

void je_log_strat();
void je_log_shutdown();

void je_init(int argc, char** argv)
{
    je_log_strat();

    wo_init(argc, argv);

    jeecs_file_set_runtime_path(wo_exe_path());

    wo_virtual_source(jeecs_towoo_component_path, jeecs_towoo_component_src, false);
    wo_virtual_source(jeecs_towoo_system_path, jeecs_towoo_system_src, false);
    wo_virtual_source(jeecs_woolang_editor_api_path, jeecs_woolang_editor_api_src, false);
    wo_virtual_source(jeecs_woolang_api_path, jeecs_woolang_api_src, false);
    wo_virtual_source(shader_wrapper_path, shader_wrapper_src, false);
    wo_virtual_source(shader_light2d_path, shader_light2d_src, false);
    wo_virtual_source(shader_pbr_path, shader_pbr_src, false);
    wo_virtual_source(gui_api_path, gui_api_src, false);
    jeal_init();

    jeecs::enrty::module_entry();
}

wo_integer_t crc64_of_source_and_api()
{
    wo_integer_t api_crc64 = wo_crc64_dir((std::string(wo_exe_path()) + "/builtin/api").c_str());
    wo_integer_t src_crc64 = wo_crc64_dir((std::string(wo_exe_path()) + "/builtin/editor").c_str());
    return src_crc64 * api_crc64;
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

bool jedbg_main_script_entry(void)
{
    bool failed_in_start_script = false;

    wo_vm vmm = try_open_cached_binary();

    if (vmm == nullptr)
    {
        if ((vmm = _jewo_open_file_to_compile_vm("@/builtin/editor/main.wo")) != nullptr)
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
        else if ((vmm = _jewo_open_file_to_compile_vm("@/builtin/main.wo")) != nullptr)
        {
            // Load normal entry.
        }
        else
        {
            failed_in_start_script = true;
        }
    }

    if (failed_in_start_script == false)
    {
        wo_run(vmm);
        wo_close_vm(vmm);
    }

    return !failed_in_start_script;
}

std::vector<void*> _free_module_list;
std::mutex _free_module_list_mx;

void je_finish()
{
    jeecs::enrty::module_leave();

    je_ecs_shutdown();
    jeal_finish();
    jegl_finish();
    jetowoo_finish();

    je_log_shutdown();
    wo_finish([](void*) 
        {
            std::lock_guard g1(_free_module_list_mx);
            for (auto* mod : _free_module_list)
                wo_unload_lib(mod);

            _free_module_list.clear();
        }, nullptr
    );
}

const char* je_build_version()
{
    return "JoyEngine 4.1.5 " __TIMESTAMP__;
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
