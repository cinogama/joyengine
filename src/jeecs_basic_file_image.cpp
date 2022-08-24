#define JE_IMPL
#include "jeecs.hpp"

#include <cstdio>
#include<sys/stat.h>

jeecs_file* jeecs_file_open(const char* path)
{
    // TODO: Open file in work path.
    FILE* fhandle = fopen(path, "rb");
    if (fhandle)
    {
        jeecs_file* jefhandle = jeecs::basic::create_new<jeecs_file>();
        jefhandle->m_native_file_handle = fhandle;

        struct stat cfstat;
        stat(path, &cfstat);
        jefhandle->m_file_length = cfstat.st_size;

        return jefhandle;
    }
    jeecs::debug::log_error("Fail to open file: '%s'.", path);
    return nullptr;
}
void jeecs_file_close(jeecs_file* file)
{
    if (file)
    {
        fclose(file->m_native_file_handle);
        jeecs::basic::destroy_free(file);
    }
}
size_t jeecs_file_read(
    void* out_buffer,
    size_t elem_size,
    size_t count,
    jeecs_file* file)
{
    return fread(out_buffer, elem_size, count, file->m_native_file_handle);
}