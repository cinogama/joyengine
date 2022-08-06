#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

extern const char* shader_wrapper_path;
extern const char* shader_wrapper_src;

extern const char* gui_api_path;
extern const char* gui_api_src;

extern const char* jeecs_woolang_api_path;
extern const char* jeecs_woolang_api_src;

extern const char* jeecs_filesys_woolang_api_path;
extern const char* jeecs_filesys_woolang_api_src;

void je_init(int argc, char** argv)
{
    je_log_strat();

    wo_init(argc, argv);
    wo_virtual_source(jeecs_woolang_api_path, jeecs_woolang_api_src, false);
    wo_virtual_source(shader_wrapper_path, shader_wrapper_src, false);
    wo_virtual_source(gui_api_path, gui_api_src, false);
    wo_virtual_source(jeecs_filesys_woolang_api_path, jeecs_filesys_woolang_api_src, false);
}

bool jedbg_editor(void)
{
    bool failed_in_start_editor = false;

    wo_vm vmm = wo_create_vm();
    if (wo_load_file(vmm, "builtin/Editor/main.wo"))
        wo_run(vmm);
    else
    {
        jeecs::debug::log_error(wo_get_compile_error(vmm, WO_NEED_COLOR));
        failed_in_start_editor = true;
    }
    wo_close_vm(vmm);

    return !failed_in_start_editor;
}

void je_finish()
{
    je_log_shutdown();
    wo_finish();
}