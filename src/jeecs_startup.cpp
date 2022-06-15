#define JE_IMPL
#include "jeecs.hpp"

extern const char* shader_wrapper_path;
extern const char* shader_wrapper_src;

extern const char* gui_api_path;
extern const char* gui_api_src;

extern const char* jeecs_woolang_api_path;
extern const char* jeecs_woolang_api_src;

void je_init(int argc, char** argv)
{
    wo_init(argc, argv);
    wo_virtual_source(jeecs_woolang_api_path, jeecs_woolang_api_src, false);
    wo_virtual_source(shader_wrapper_path, shader_wrapper_src, false);
    wo_virtual_source(gui_api_path, gui_api_src, false);

    bool failed_in_start_editor = false;

    wo_vm vmm = wo_create_vm();
    if (wo_load_file(vmm, "builtin/Editor/main.wo"))
        wo_run(vmm);
    if (wo_has_compile_warning(vmm))
        jeecs::debug::log_warn(wo_get_compile_warning(vmm, WO_NEED_COLOR));
    if (wo_has_compile_error(vmm))
    {
        jeecs::debug::log_error(wo_get_compile_error(vmm, WO_NEED_COLOR));
        failed_in_start_editor = true;
    }
    wo_close_vm(vmm);

    if (failed_in_start_editor)
    {
        wo_finish();
        exit(-1);
    }
}

void je_finish()
{
    wo_finish();
}