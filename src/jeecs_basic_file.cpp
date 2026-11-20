#define JE_IMPL
#include "jeecs/file.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs/logging.hpp"

#include "jeecs_basic_file.hpp"
#include "jeecs_basic_bootup.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

namespace jeecs::file {}

using namespace std;
using namespace jeecs;
using namespace jeecs::file;

namespace jeecs::file
{
    namespace
    {
        // Application identity for the per-user data directory, derived from
        // argv[0] (the executable path as launched). Returns an empty string
        // when no usable argv is available (e.g. Android calls je_core_init
        // with argc == 0); special platforms (Android/WebGL) set the final
        // user path in their ports, where the executable name is meaningless.
        string _application_name(int argc, /* OPTIONAL: If argc == 0 */ char** argv)
        {
            if (argc <= 0 || argv == nullptr || argv[0] == nullptr)
                return {};

            string name = argv[0];

            const size_t separator = name.find_last_of("/\\");
            if (separator != string::npos)
                name.erase(0, separator + 1);

            if (name.size() > 4)
            {
                const char* const extension = name.c_str() + name.size() - 4;
                if (extension[0] == '.'
                    && (extension[1] == 'e' || extension[1] == 'E')
                    && (extension[2] == 'x' || extension[2] == 'X')
                    && (extension[3] == 'e' || extension[3] == 'E'))
                {
                    name.resize(name.size() - 4);
                }
            }

            return name;
        }

        // OS-standard per-user data directory for this application, or an
        // empty string on platforms that do not use the environment scheme
        // (Android/WebGL set the final path in their ports).
        string _default_user_path(int argc, char** argv)
        {
            string base;
#if defined(_WIN32)
            if (const char* appdata = getenv("APPDATA");
                appdata != nullptr && appdata[0] != '\0')
                base = string(appdata) + "/JoyEngine";
#elif defined(__APPLE__)
            if (const char* home = getenv("HOME");
                home != nullptr && home[0] != '\0')
                base = string(home) + "/Library/Application Support/JoyEngine";
#elif defined(__linux__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
            // XDG Base Directory spec: a relative XDG_DATA_HOME is ignored.
            if (const char* xdg_data_home = getenv("XDG_DATA_HOME");
                xdg_data_home != nullptr && xdg_data_home[0] == '/')
                base = string(xdg_data_home) + "/JoyEngine";
            else if (const char* home = getenv("HOME");
                home != nullptr && home[0] != '\0')
                base = string(home) + "/.local/share/JoyEngine";
#endif
            if (base.empty())
                return {};

            const string application_name = _application_name(argc, argv);
            return application_name.empty() ? base : base + "/" + application_name;
        }
    }

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

    void bootup(int argc, char** argv)
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

#ifdef JE4_SPECIFY_HOST_RELATIVE_PATH
        // Packaged layout: data (pkg/builtin/je/licenses, fimg archives) lives
        // at a fixed relative offset from the executable directory, e.g.
        // Contents/Resources/ in a macOS .app bundle, where binaries stay in
        // Contents/MacOS/ (sibling dylibs are located through
        // @executable_path rpath). Entry scripts are resolved via the runtime
        // path ("@/builtin/main.wo"), builtin resources via the host path
        // ("!/builtin/...").
        exe_path += "/" JE4_SPECIFY_HOST_RELATIVE_PATH;
        
        // Overwrite woort path.
        if (!woort_set_exe_path(exe_path.c_str()))
            debug::logwarn("Failed to update woort path.");

#endif
        je_file_host_path_set(exe_path.c_str());
        je_file_runtime_path_set(exe_path.c_str());
        je_fimg_default_archive_set(exe_path.c_str());

        // User data path: the per-user OS-standard writable directory for
        // user state (editor window layout, project open history). Install
        // layouts such as a macOS .app keep the host path read-only, so user
        // state must never target it. User state is per-application, so the
        // executable name is appended as a subdirectory: independent games do
        // not overwrite each other's layouts or history, and uninstallers can
        // remove just their own subtree. Special platforms (Android/WebGL) do
        // not use the environment scheme and keep the exe-dir default until
        // their ports set the final path.
        string user_path = _default_user_path(argc, argv);
        if (user_path.empty())
            user_path = exe_path;

        je_file_user_path_set(user_path.c_str());
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
