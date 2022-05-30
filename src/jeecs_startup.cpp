#define JE_IMPL
#include "jeecs.hpp"


extern const char* shader_wrapper_path;
extern const char* shader_wrapper_src;

extern const char* gui_api_path;
extern const char* gui_api_src;

void je_init(int argc, char** argv)
{
    rs_init(argc, argv);
    rs_virtual_source(shader_wrapper_path, shader_wrapper_src, false);
    rs_virtual_source(gui_api_path, gui_api_src, false);
}

void je_finish()
{
    rs_finish();
}