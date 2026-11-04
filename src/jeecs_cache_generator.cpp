#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_cache_generator.hpp"

#include "wo.h"

#include <cstdio>
#include <string>
#include <vector>

namespace jeecs
{
    namespace cache
    {
        std::string resolve_source_path(const char* filepath)
        {
            assert(filepath != nullptr);

            if (filepath[0] == '@')
            {
                std::string root = jeecs_file_get_runtime_path();
                if (root.empty())
                    root = jeecs_file_get_host_path();

                const char* rest = filepath + 1;
                if (root.empty())
                    return rest;

                const bool root_sep = root.back() == '/' || root.back() == '\\';
                const bool rest_sep = rest[0] == '/' || rest[0] == '\\';
                if (root_sep && rest_sep)
                    return root + (rest + 1);
                if (!root_sep && !rest_sep)
                    return root + "/" + rest;
                return root + rest;
            }

            if (filepath[0] == '!')
            {
                std::string root = jeecs_file_get_host_path();
                const char* rest = filepath + 1;
                if (root.empty())
                    return rest;

                const bool root_sep = root.back() == '/' || root.back() == '\\';
                const bool rest_sep = rest[0] == '/' || rest[0] == '\\';
                if (root_sep && rest_sep)
                    return root + (rest + 1);
                if (!root_sep && !rest_sep)
                    return root + "/" + rest;
                return root + rest;
            }

            return filepath;
        }

        bool generate_from_script(
            const char* source_filepath,
            uint32_t format_version,
            const char* generator_script_path,
            const char* generator_entry)
        {
            assert(source_filepath != nullptr);
            assert(generator_script_path != nullptr);
            assert(generator_entry != nullptr);

            jeecs_file* script_file = jeecs_file_open(generator_script_path);
            if (script_file == nullptr)
            {
                jeecs::debug::logerr(
                    "Cannot open cache generator '%s'.", generator_script_path);
                return false;
            }

            const size_t script_size = jeecs_file_size(script_file);
            std::vector<char> script_source(script_size + 1);
            if (script_size > 0
                && script_size != jeecs_file_read(
                    script_source.data(), sizeof(char), script_size, script_file))
            {
                jeecs_file_close(script_file);
                jeecs::debug::logerr(
                    "Failed to read cache generator '%s'.", generator_script_path);
                return false;
            }
            script_source[script_size] = '\0';
            jeecs_file_close(script_file);

            wo_CompileErrors* cerror = nullptr;
            woort_CodeEnv* const cenv = wo_load_source(
                generator_script_path, script_source.data(), &cerror);
            if (cenv == nullptr)
            {
                jeecs::debug::logerr(
                    "Failed to compile cache generator '%s':\n%s",
                    generator_script_path,
                    wo_get_compile_error(cerror, WO_PLAIM));

                wo_compile_errors_free(cerror);
                return false;
            }

            // 生成器第一个参数是缓存写入器，第二个参数是源 TOML 的本地路径。
            // 缓存文件头由 C 侧创建，脚本只负责写二进制文档。
            const std::string source_path = resolve_source_path(source_filepath);
            je_writing_cache_file* const cache_file = jeecs_create_cache_file(
                source_filepath, format_version, 0);
            if (cache_file == nullptr)
            {
                woort_codeenv_drop(cenv);
                return false;
            }

            bool generated = false;

            woort_vm* const vmm = woort_vm_create();
            if (vmm == nullptr)
                jeecs::debug::logerr(
                    "Failed to create vm for cache generator '%s'.",
                    generator_script_path);
            else
            {
                woort_vm* const last = woort_vm_swap(vmm);
                {
                    if (WOORT_VM_CALL_STATUS_NORMAL
                        != woort_bootup(WOORT_IGNORE, cenv, false))
                    {
                        jeecs::debug::logerr(
                            "Failed to init cache generator '%s': %s.",
                            generator_script_path,
                            woort_vm_get_runtime_error(vmm));
                    }
                    else
                    {
                        woort_value stack;
                        if (!woort_push_reserve(3, &stack))
                            jeecs::debug::logerr(
                                "Failed to reserve stack for cache generator.");
                        else
                        {
                            const woort_value arg_cache = stack + 0;
                            const woort_value arg_source = stack + 1;
                            const woort_value func = stack + 2;

                            woort_set_pointer(arg_cache, cache_file);
                            woort_set_string(arg_source, source_path.c_str());

                            if (!woort_load_extern_const(
                                func, cenv, generator_entry))
                            {
                                jeecs::debug::logerr(
                                    "Cache generator '%s' has no '%s' function.",
                                    generator_script_path,
                                    generator_entry);
                            }
                            else if (WOORT_VM_CALL_STATUS_NORMAL
                                != woort_invoke(arg_cache, func))
                            {
                                jeecs::debug::logerr(
                                    "Cache generation failed for '%s': %s.",
                                    source_filepath,
                                    woort_vm_get_runtime_error(vmm));
                            }
                            else
                                generated = woort_bool(arg_cache);

                            woort_pop(3);
                        }
                    }
                }
                (void)woort_vm_swap(last);
                woort_vm_close(vmm);
            }

            jeecs_close_cache_file(cache_file);

            if (generated)
            {
                if (jeecs_file* cache = jeecs_load_cache_file(
                    source_filepath, format_version, 0))
                {
                    jeecs_file_close(cache);
                    woort_codeenv_drop(cenv);
                    return true;
                }
            }

            const std::string cache_path = source_path + ".je4cache";
            std::remove(cache_path.c_str());

            woort_codeenv_drop(cenv);
            return false;
        }
    }
}
