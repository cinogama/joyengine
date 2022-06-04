#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_editor.hpp"


extern const char* shader_wrapper_path;
extern const char* shader_wrapper_src;

extern const char* gui_api_path;
extern const char* gui_api_src;

extern const char* jeecs_editor_api_path;
extern const char* jeecs_editor_api_src;

void je_init(int argc, char** argv)
{
    rs_init(argc, argv);
    rs_virtual_source(jeecs_editor_api_path, jeecs_editor_api_src, false);
    rs_virtual_source(shader_wrapper_path, shader_wrapper_src, false);
    rs_virtual_source(gui_api_path, gui_api_src, false);

    bool failed_in_start_editor = false;
    rs_vm vmm = rs_create_vm();
    if (rs_load_file(vmm, "builtin/Editor/main.rsn"))
        rs_run(vmm);
    if (rs_has_compile_warning(vmm))
        jeecs::debug::log_warn(rs_get_compile_warning(vmm, RS_NEED_COLOR));
    if (rs_has_compile_error(vmm))
    {
        jeecs::debug::log_error(rs_get_compile_error(vmm, RS_NEED_COLOR));
        failed_in_start_editor = true;
    }
    rs_close_vm(vmm);

    if (failed_in_start_editor)
    {
        rs_finish();
        exit(-1);
    }
}

void je_finish()
{
    rs_finish();
}