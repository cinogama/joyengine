#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_basic_file.hpp"
#include "jeecs_basic_bootup.hpp"

#include <cstdio>
#include <string>

namespace jeecs::file {}

using namespace std;
using namespace jeecs;
using namespace jeecs::file;

namespace jeecs::file
{
    namespace native
    {
        namespace
        {
            je_read_file_open_func_t open_impl;
            je_read_file_func_t read_impl;
            je_read_file_tell_func_t tell_impl;
            je_read_file_seek_func_t seek_impl;
            je_read_file_close_func_t close_impl;

            /* Default functions */
            namespace default_functions
            {
                je_raw_file_t open(const char* path, size_t* out_len)
                {
                    FILE* fhandle = fopen(path, "rb");
                    if (fhandle)
                    {
#if defined(_WIN32)
                        struct _stat64 cfstat;
                        const int stat_result = _stat64(path, &cfstat);
#else
                        struct stat cfstat;
                        const int stat_result = stat(path, &cfstat);
#endif
                        if (stat_result != 0)
                        {
                            fclose(fhandle);
                            return nullptr;
                        }
                        *out_len = static_cast<size_t>(cfstat.st_size);
                    }
                    return reinterpret_cast<je_raw_file_t>(fhandle);
                }
                size_t read(void* buffer, size_t elemsz, size_t elemcount, je_raw_file_t file)
                {
                    return fread(buffer, elemsz, elemcount, reinterpret_cast<FILE*>(file));
                }
                size_t tell(je_raw_file_t file)
                {
#if defined(_WIN32)
                    return static_cast<size_t>(_ftelli64(reinterpret_cast<FILE*>(file)));
#else
                    return static_cast<size_t>(ftello(reinterpret_cast<FILE*>(file)));
#endif
                }
                int seek(je_raw_file_t file, int64_t offset, je_file_seek_mode_t mode)
                {
#if defined(_WIN32)
                    return _fseeki64(
                        reinterpret_cast<FILE*>(file),
                        offset,
                        static_cast<int>(mode));
#else
                    return fseeko(
                        reinterpret_cast<FILE*>(file),
                        static_cast<off_t>(offset),
                        static_cast<int>(mode));
#endif
                }
                int close(je_raw_file_t file)
                {
                    return fclose(reinterpret_cast<FILE*>(file));
                }
            }
        }

        je_raw_file_t open(const char* path, size_t* out_len)
        {
            return open_impl(path, out_len);
        }
        size_t read(void* buffer, size_t elemsz, size_t elemcount, je_raw_file_t file)
        {
            return read_impl(buffer, elemsz, elemcount, file);
        }
        size_t tell(je_raw_file_t file)
        {
            return tell_impl(file);
        }
        int seek(je_raw_file_t file, int64_t offset, je_file_seek_mode_t mode)
        {
            return seek_impl(file, offset, mode);
        }
        int close(je_raw_file_t file)
        {
            return close_impl(file);
        }
    }

    void bootup(int, char**)
    {
        using namespace jeecs::file::native;
        using namespace jeecs::file::native;

        open_impl = &default_functions::open;
        read_impl = &default_functions::read;
        tell_impl = &default_functions::tell;
        seek_impl = &default_functions::seek;
        close_impl = &default_functions::close;

        string exe_path = {};
        const size_t need = woort_exe_path(nullptr, 0);
        if (need != 0)
        {
            exe_path.resize(need);
            (void)woort_exe_path(exe_path.data(), need);
        }

        je_file_host_path_set(exe_path.c_str());
        je_file_runtime_path_set(exe_path.c_str());
        je_fimg_default_archive_set(exe_path.c_str());
    }
    void shutdown()
    {
        je_fimg_default_archive_set(nullptr);
    }
}

void je_file_native_operator_register(
    je_read_file_open_func_t opener,
    je_read_file_func_t reader,
    je_read_file_tell_func_t teller,
    je_read_file_seek_func_t seeker,
    je_read_file_close_func_t closer)
{
    assert(opener != nullptr
        && reader != nullptr
        && teller != nullptr
        && seeker != nullptr
        && closer != nullptr);

    native::open_impl = opener;
    native::read_impl = reader;
    native::tell_impl = teller;
    native::seek_impl = seeker;
    native::close_impl = closer;
}
