#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs/cache.hpp"
#include "jeecs/core.hpp"
#include "jeecs/entry.hpp"
#include "jeecs/file.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/module.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs/typing.hpp"
#include "jeecs/version.hpp"
#include "jeecs_basic_bootup.hpp"
#include "jeecs_basic_cache_version.hpp"

#include "wo.h"

#include <list>

namespace jeecs::core {}

using namespace std;
using namespace jeecs;
using namespace jeecs::core;

namespace jeecs::core
{
    namespace
    {
        /* OPTIONAL */ woort_CodeEnv* _je_open_binary_to_compile_vm(
            const char* vpath, const void* buf, size_t buf_len)
        {
            wo_CompileErrors* cerror;
            woort_CodeEnv* const cenv =
                wo_load_binary(vpath, buf, buf_len, &cerror);

            if (cenv != nullptr)
                return cenv;

            debug::logwarn("Failed to load & create woolang source '%s':\n%s",
                vpath,
                wo_get_compile_error(cerror, WO_COLORFUL));

            wo_compile_errors_free(cerror);
            return nullptr;
        }
        /* OPTIONAL */ woort_CodeEnv* _je_open_file_to_compile_vm(const char* vpath)
        {
            auto* src_file_handle = je_file_open(vpath);
            if (src_file_handle == nullptr)
                return nullptr;

            vector<char> src_buffer(je_file_size(src_file_handle));
            je_file_read(src_buffer.data(), sizeof(char), src_buffer.size(), src_file_handle);
            je_file_close(src_file_handle);

            return _je_open_binary_to_compile_vm(vpath, src_buffer.data(), src_buffer.size());
        }
        /* OPTIONAL */ woort_CodeEnv* _je_try_open_cached_binary(const char* vpath)
        {
            auto* const cache = je_cache_file_load(
                vpath,
                EDITOR_SCRIPT_CACHE_VERSION,
                EDITOR_SCRIPT_CACHE_VERSION);

            if (cache == nullptr)
                return nullptr;

            woort_CodeEnv* cenv = nullptr;
            do
            {
                je_cache_binary_document_t doc;
                if (!je_cache_binary_document_open(cache, &doc))
                    break;

                je_cache_binary_buffer_t buf;
                if (!je_cache_binary_document_index(
                    &doc,
                    "content",
                    je_cache_binary_value_type_t::JE_CACHE_BINARY_VALUE_BUFFER,
                    &buf))
                    break;

                size_t len = 0;
                if (!je_cache_binary_buffer_read(nullptr, &len, &buf))
                    break;

                vector<char> src_buffer(len);
                if (!je_cache_binary_buffer_read(src_buffer.data(), &len, &buf))
                    break;

                cenv = _je_open_binary_to_compile_vm(
                    vpath, src_buffer.data(), src_buffer.size());

            } while (0);

            je_file_close(cache);
            return cenv;
        }
    }
}

void je_core_init(int argc, /* OPTIONAL: If argc == 0 */ char** argv)
{
    bootup(argc, argv);
}

void je_core_finish()
{
    shutdown();
}

bool je_core_main_script_entry()
{
    bool failed_in_start_script = false;

    woort_CodeEnv* cenv = nullptr;
    if ((cenv = _je_open_file_to_compile_vm("@/builtin/main.wo")) != nullptr)
    {
        // Load normal entry.
    }
    else if ((cenv = _je_try_open_cached_binary("@/builtin/editor/main.wo")) != nullptr)
    {
        // Cache loaded, skip,
    }
    else if ((cenv = _je_open_file_to_compile_vm(
        /* NOTE: Use je_file_host_path_get() to make sure woolang can find import target correctly. */
        (string(je_file_host_path_get()) + "/builtin/editor/main.wo").c_str())) != nullptr)
    {
        size_t binary_length;
        void* buffer;

        if (woort_CodeEnv_save_binary(cenv, &buffer, &binary_length))
        {
            auto* const cache = je_cache_file_create(
                "!/builtin/editor/main.wo", 
                EDITOR_SCRIPT_CACHE_VERSION, 
                EDITOR_SCRIPT_CACHE_VERSION);

            bool succ = false;
            if (cache != nullptr) do
            {
                if (!je_cache_binary_append_document(cache))
                    break;
                if (!je_cache_binary_append_string(cache, "content"))
                    break;
                if (!je_cache_binary_append_buffer(cache, buffer, binary_length))
                    break;
                if (!je_cache_binary_append_end(cache))
                    break;

                succ = true;
            } while (0);

            if (!succ)
                debug::logwarn("Failed to create editor script binary cache.");

            if (cache != nullptr)
                je_cache_file_close(cache, succ);

            woort_free(buffer);
        }
    }
    else
    {
        failed_in_start_script = true;
    }

    if (failed_in_start_script == false)
    {
        woort_vm* const vmm = woort_vm_create();
        if (vmm == nullptr)
            debug::logerr("Failed to create vm instance.");
        else
        {
            woort_vm* const last = woort_vm_swap(vmm);
            {
                (void)woort_bootup(WOORT_IGNORE, cenv, true);
            }
            (void)woort_vm_swap(last);

            woort_vm_close(vmm);
        }

        woort_codeenv_drop(cenv);
    }

    return !failed_in_start_script;
}

const char* je_build_version()
{
#define JE_VERSION_WRAP(A, B, C) #A "." #B "." #C
    return "JoyEngine " JE_CORE_VERSION " " __TIMESTAMP__;
#undef JE_VERSION_WRAP
}

const char* je_build_commit()
{
    return
#if __has_include("jeecs_commit_sha.hpp")
#   include "jeecs_commit_sha.hpp"
#else
        "untracked"
#endif
        ;
}

/* OPTIONAL: If load failed */ woort_Dylib* je_module_load(const char* name, const char* path)
{
    if (woort_Dylib* lib = woort_dylib_load(name, path, nullptr, false))
    {
        if (auto entry = (typing::module_entry_t)
            woort_dylib_load_func(lib, "jeecs_module_entry"))
            entry(lib);

        debug::loginfo("Module: '%s'(%p) loaded", path, lib);
        return lib;
    }
    debug::logerr("Failed to load module: '%s'", path);
    return nullptr;
}

/* OPTIONAL: If symbol not found */ void* je_module_func(woort_Dylib* lib, const char* funcname)
{
    assert(lib);
    return woort_dylib_load_func(lib, funcname);
}

void je_module_unload(woort_Dylib* lib)
{
    assert(lib);
    if (auto leave = (typing::module_leave_t)woort_dylib_load_func(lib, "jeecs_module_leave"))
        leave();
    debug::loginfo("Module: '%p' request to unload.", lib);
    woort_dylib_unload(lib, WOORT_DYLIB_UNREF_AND_BURY);
}
