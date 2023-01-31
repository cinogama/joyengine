#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

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

void jeal_init();
void jeal_finish();

void je_init(int argc, char** argv)
{
    je_log_strat();

    wo_init(argc, argv);
    wo_enable_jit(false);

    wo_virtual_source(jeecs_woolang_api_path, jeecs_woolang_api_src, false);
    wo_virtual_source(shader_wrapper_path, shader_wrapper_src, false);
    wo_virtual_source(shader_light2d_path, shader_light2d_src, false);
    wo_virtual_source(shader_pbr_path, shader_pbr_src, false);
    wo_virtual_source(gui_api_path, gui_api_src, false);

    jeal_init();
}

wo_integer_t crc64_of_source_and_api()
{
    wo_integer_t api_crc64 = wo_crc64_dir((std::string(wo_exe_path()) + "/builtin/api").c_str());
    wo_integer_t src_crc64 = wo_crc64_dir((std::string(wo_exe_path()) + "/builtin/editor").c_str());
    return src_crc64 * api_crc64;
}

wo_vm try_open_cached_binary()
{
    wo_integer_t expect_crc = 0;
    FILE* srccrc = fopen((std::string(wo_exe_path()) + "/builtin/editor.crc.jecache4").c_str(), "rb");
    if (srccrc == nullptr)
        return nullptr;

    size_t readcount = fread(&expect_crc, sizeof(expect_crc), 1, srccrc);
    fclose(srccrc);

    if (readcount < 1)
        return nullptr;

    if (crc64_of_source_and_api() != expect_crc)
        return nullptr;

    wo_vm vmm = wo_create_vm();
    if (wo_load_file(vmm, (std::string(wo_exe_path()) + "/builtin/editor.woo.jecache4").c_str()))
        return vmm;

    jeecs::debug::logwarn("Failed to load editor compile cache:\n%s",
        wo_get_compile_error(vmm, WO_NEED_COLOR));

    wo_close_vm(vmm);
    return nullptr;
}

bool jedbg_editor(void)
{
    bool failed_in_start_editor = false;

    wo_vm vmm = try_open_cached_binary();

    if (vmm == nullptr)
    {
        vmm = wo_create_vm();
        if (wo_load_file(vmm, (std::string(wo_exe_path()) + "/builtin/editor/main.wo").c_str()))
        {
            size_t binary_length;
            void* buffer = wo_dump_binary(vmm, &binary_length);;

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
            jeecs::debug::logerr(wo_get_compile_error(vmm, WO_NEED_COLOR));
            failed_in_start_editor = true;
        }
    }

    if (failed_in_start_editor == false)
        wo_run(vmm);

    wo_close_vm(vmm);

    return !failed_in_start_editor;
}

void je_finish()
{
    jeal_finish();

    je_log_shutdown();
    wo_finish();
}

const char* je_build_version()
{
    return "JoyEngine 4.0.dev " __TIMESTAMP__;
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
    jeecs::debug::loginfo("Module: '%p' unloaded", lib);
    return wo_unload_lib(lib);
}
