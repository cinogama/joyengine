#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/file.hpp"

#include <cstddef>
#include <cstdint>

namespace jeecs::file
{
    namespace native
    {
        je_raw_file_t open(const char* path, size_t* out_len);
        size_t read(void* buffer, size_t elemsz, size_t elemcount, je_raw_file_t file);
        size_t tell(je_raw_file_t file);
        int seek(je_raw_file_t file, int64_t offset, je_file_seek_mode_t mode);
        int close(je_raw_file_t file);
    }
}