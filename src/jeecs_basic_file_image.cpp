#define JE_IMPL
#include "jeecs.hpp"

#include <cstdio>
#include<sys/stat.h>

jeecs_file* jeecs_file_open(const char* path)
{
    // TODO: Open file in work path.
    std::string path_str = path;
    if (path[0] == '*')
        path_str = wo_exe_path() + path_str.substr(1);

    FILE* fhandle = fopen(path_str.c_str(), "rb");
    if (fhandle)
    {
        jeecs_file* jefhandle = jeecs::basic::create_new<jeecs_file>();
        jefhandle->m_native_file_handle = fhandle;

        struct stat cfstat;
        stat(path_str.c_str(), &cfstat);
        jefhandle->m_file_length = cfstat.st_size;

        return jefhandle;
    }
    jeecs::debug::logerr("Fail to open file: '%s'(%d).", path, (int)errno);
    return nullptr;
}
void jeecs_file_close(jeecs_file* file)
{
    assert(file != nullptr && file->m_native_file_handle != nullptr);
    auto close_state = fclose(file->m_native_file_handle);

    if (close_state != 0)
        jeecs::debug::logerr("Fail to close file(%d:%d).", (int)close_state, (int)errno);

    jeecs::basic::destroy_free(file);
}
size_t jeecs_file_read(
    void* out_buffer,
    size_t elem_size,
    size_t count,
    jeecs_file* file)
{
    return fread(out_buffer, elem_size, count, file->m_native_file_handle);
}