#define JE_IMPL
#include "jeecs.hpp"


extern const char* shader_wrapper_path;
extern const char* shader_wrapper_src;

void je_init(int argc, char** argv)
{
    rs_init(argc, argv);

    at_quick_exit(rs_finish);
    atexit(rs_finish);

    rs_virtual_source(shader_wrapper_path, shader_wrapper_src, false);
}

void je_finish()
{

}