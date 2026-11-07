#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_basic_bootup.hpp"
#include "jeecs_cache_version.hpp"

#include "wo.h"

#include <string>
#include <vector>

namespace jeecs::internal_script {}

using namespace std;
using namespace jeecs;
using namespace jeecs::internal_script;

namespace jeecs::internal_script
{
    namespace
    {
        optional<woort_CodeEnv*> try_load_internal_script_with_cache(const char* path)
        {
            auto* const cache_file = je_cache_file_load(path, INTERNAL_SCRIPT_CACHE_VERSION, 0);
            woort_CodeEnv* cenv = nullptr;

            if (cache_file != nullptr)
            {
                do
                {
                    je_cache_binary_document_t doc;
                    if (!je_cache_binary_document_open(cache_file, &doc))
                        break;

                    je_cache_binary_buffer_t content;
                    if (!je_cache_binary_document_index(
                        &doc,
                        "content",
                        je_cache_binary_value_type_t::JE_CACHE_BINARY_VALUE_BUFFER,
                        &content))
                    {
                        // Bad cache.
                        break;
                    }

                    // Ok, trying to load binary buffer.
                    size_t length = 0;
                    if (!je_cache_binary_buffer_read(nullptr, &length, &content))
                        break;

                    vector<char> binary_content(length);
                    if (!je_cache_binary_buffer_read(binary_content.data(), &length, &content))
                        break;

                    wo_CompileErrors* compile_errors;
                    cenv = wo_load_binary(path, binary_content.data(), length, &compile_errors);
                    if (cenv == nullptr)
                    {
                        // Loading binary failed.
                        debug::logwarn("Failed to load internal script cache: \n%s",
                            wo_get_compile_error(compile_errors, wo_inform_style_t::WO_PLAIM));

                        wo_compile_errors_free(compile_errors);
                        break;
                    }

                    // Finish.

                } while (0);

                // Close cache file.
                je_file_close(cache_file);

                if (cenv != nullptr)
                    return cenv;

                debug::logwarn("Bad internal script cache while loading `%s`, retry with source.", path);
            }

            auto* const file = je_file_open(path);
            if (file == nullptr)
            {
                debug::log("Unable to load internal script: `%s`, skip.", path);
                return nullopt;
            }

            const size_t source_length = je_file_size(file);
            vector<char> source_content(source_length);

            if (source_length != je_file_read(source_content.data(), sizeof(char), source_length, file))
            {
                debug::logwarn("Failed to read internal script `%s`.", path);
                je_file_close(file);

                return nullopt;
            }
            je_file_close(file);

            wo_CompileErrors* compile_errors;
            cenv = wo_load_binary(path, source_content.data(), source_length, &compile_errors);
            if (cenv == nullptr)
            {
                // Loading binary failed.
                debug::logerr("Failed to compile internal script: \n%s",
                    wo_get_compile_error(compile_errors, wo_inform_style_t::WO_PLAIM));

                wo_compile_errors_free(compile_errors);
                return nullopt;
            }

            // Save binary to cache.
            void* binary_buffer;
            size_t binary_length;

            if (!woort_CodeEnv_save_binary(cenv, &binary_buffer, &binary_length))
                debug::logwarn("Failed to save cache file for internal script `%s`.", path);

            auto* const writting_cache = je_cache_file_create(path, INTERNAL_SCRIPT_CACHE_VERSION, 0);
            bool cache_create_succ = false;

            if (writting_cache != nullptr) do
            {
                if (!je_cache_binary_append_document(writting_cache))
                    break;

                if (!je_cache_binary_append_string(writting_cache, "content"))
                    break;

                if (!je_cache_binary_append_buffer(writting_cache, binary_buffer, binary_length))
                    break;

                if (!je_cache_binary_append_end(writting_cache))
                    break;

                cache_create_succ = true;

            } while (0);

            if (!cache_create_succ)
                debug::logwarn("Failed to create cache file for internal script `%s`.", path);

            woort_free(binary_buffer);

            if (writting_cache != nullptr)
                je_cache_file_close(writting_cache, cache_create_succ);

            return cenv;
        }
        bool try_bootup_internal_script_cenv(const char* path, woort_CodeEnv* cenv)
        {
            assert(cenv != nullptr);

            woort_vm* const vmm = woort_vm_create();

            if (vmm == nullptr)
            {
                debug::logerr("Failed to create new script vm.");
                return false;
            }

            bool succ = true;
            auto* const last_vm = woort_vm_swap(vmm);
            {
                if (WOORT_VM_CALL_STATUS_NORMAL != woort_bootup(WOORT_IGNORE, cenv, true))
                {
                    debug::logerr("Failed to bootup internal script `%s`.", path);
                    succ = false;
                }
            }
            (void)woort_vm_swap(last_vm);
            woort_vm_close(vmm);

            return succ;
        }
        optional<woort_CodeEnv*> try_load_internal_script_with_cache_and_bootup(const char* path)
        {
            auto code_env_may_nullopt = try_load_internal_script_with_cache(path);
            if (code_env_may_nullopt.has_value())
            {
                woort_CodeEnv* const cenv = code_env_may_nullopt.value();

                if (try_bootup_internal_script_cenv(path, cenv))
                    return cenv;

                woort_codeenv_drop(cenv);
            }
            return nullopt;
        }

        optional<woort_CodeEnv*> cache_builder_of_animation = nullopt;
        optional<woort_CodeEnv*> cache_builder_of_tilemap = nullopt;
        optional<woort_CodeEnv*> cache_builder_of_tileset = nullopt;
        optional<woort_CodeEnv*> cache_builder_of_image_texture = nullopt;

        // usecrc64 follows je_cache_file_create: 0 derives the key from path's bytes,
        // any other value is the key itself (used both to create and to reload).
        static je_file_t* generate_cache(
            const char* path, uint32_t format_version, woort_CodeEnv* cenv, uint64_t usecrc64)
        {
            static constexpr const char* SCRIPT_ENTRY = "generate";

            woort_vm* const vmm = woort_vm_create();

            if (vmm == nullptr)
            {
                debug::logerr("Failed to create new script vm for generating `%s`.", path);
                return nullptr;
            }

            je_writing_cache_file_t* const cache_file = je_cache_file_create(
                path, format_version, usecrc64);

            if (cache_file == nullptr)
            {
                debug::logerr("Failed to create cache file for generating `%s`.", path);
                woort_vm_close(vmm);
                return nullptr;
            }

            bool succ = true;
            auto* const last_vm = woort_vm_swap(vmm);
            do
            {
                woort_value stack;
                if (!woort_push_reserve(3, &stack))
                {
                    debug::logerr(
                        "Failed to reserve stack for cache generator.");
                    succ = false;

                    break;
                }

                const woort_value arg_cache = stack + 0;
                const woort_value arg_source = stack + 1;
                const woort_value func = stack + 2;

                woort_set_pointer(arg_cache, cache_file);
                woort_set_string(arg_source, path);
                if (!woort_load_extern_const(
                    func, cenv, SCRIPT_ENTRY))
                {
                    debug::logerr(
                        "Failed to find function `%s` while generate cache for `%s`.",
                        SCRIPT_ENTRY,
                        path);
                    succ = false;

                    break;
                }

                if (WOORT_VM_CALL_STATUS_NORMAL
                    != woort_invoke(arg_cache, func))
                {
                    debug::logerr(
                        "Cache generation failed for '%s': %s.",
                        path,
                        woort_vm_get_runtime_error(vmm));
                    succ = false;

                    break;
                }
                succ = woort_bool(arg_cache);

            } while (0);

            (void)woort_vm_swap(last_vm);
            woort_vm_close(vmm);

            je_cache_file_close(cache_file, succ);

            if (succ)
                return je_cache_file_load(path, format_version, usecrc64);

            return nullptr;
        }
    }

    void bootup()
    {
        cache_builder_of_animation =
            try_load_internal_script_with_cache_and_bootup(
                "!/builtin/script/internal/cache/animation.wo");
        cache_builder_of_tilemap =
            try_load_internal_script_with_cache_and_bootup(
                "!/builtin/script/internal/cache/tilemap.wo");
        cache_builder_of_tileset =
            try_load_internal_script_with_cache_and_bootup(
                "!/builtin/script/internal/cache/tileset.wo");
        cache_builder_of_image_texture =
            try_load_internal_script_with_cache_and_bootup(
                "!/builtin/script/internal/cache/texture.wo");
    }
    void shutdown()
    {
        if (cache_builder_of_animation.has_value())
        {
            woort_codeenv_drop(cache_builder_of_animation.value());
            cache_builder_of_animation.reset();
        }
        if (cache_builder_of_tilemap.has_value())
        {
            woort_codeenv_drop(cache_builder_of_tilemap.value());
            cache_builder_of_tilemap.reset();
        }
        if (cache_builder_of_tileset.has_value())
        {
            woort_codeenv_drop(cache_builder_of_tileset.value());
            cache_builder_of_tileset.reset();
        }
        if (cache_builder_of_image_texture.has_value())
        {
            woort_codeenv_drop(cache_builder_of_image_texture.value());
            cache_builder_of_image_texture.reset();
        }
    }
}

/* OPTIONAL */ je_file_t* je_animation_cache_open_or_create(const char* path)
{
    if (je_file_t* file = je_cache_file_load(path, ANIMATION_CACHE_VERSION, 0))
        return file;

    if (cache_builder_of_animation.has_value())
        return generate_cache(
            path,
            ANIMATION_CACHE_VERSION,
            cache_builder_of_animation.value(),
            0);
    return nullptr;
}

/* OPTIONAL */ je_file_t* je_tilemap_cache_open_or_create(const char* path)
{
    if (je_file_t* file = je_cache_file_load(path, TILEMAP_CACHE_VERSION, 0))
        return file;

    if (cache_builder_of_tilemap.has_value())
        return generate_cache(
            path,
            TILEMAP_CACHE_VERSION,
            cache_builder_of_tilemap.value(),
            0);
    return nullptr;
}

/* OPTIONAL */ je_file_t* je_tileset_cache_open_or_create(const char* path)
{
    if (je_file_t* file = je_cache_file_load(path, TILESET_CACHE_VERSION, 0))
        return file;

    if (cache_builder_of_tileset.has_value())
        return generate_cache(
            path,
            TILESET_CACHE_VERSION,
            cache_builder_of_tileset.value(),
            0);
    return nullptr;
}

/* OPTIONAL */ je_file_t* je_image_texture_cache_open_or_create(const char* path)
{
    // The cache carries both the image pixels and the color space from the
    // optional <path>.je4texture source, so the key chains both files: a
    // change to either invalidates it. With neither file present (exported
    // runtimes ship only the cache) the key is 0 and validation is skipped.
    const uint64_t crc64_key = je_file_crc64(
        (string(path) + ".je4texture").c_str(),
        je_file_crc64(path, 0));

    if (je_file_t* file = je_cache_file_load(path, IMAGE_TEXTURE_CACHE_VERSION, crc64_key))
        return file;

    if (cache_builder_of_image_texture.has_value())
        return generate_cache(
            path,
            IMAGE_TEXTURE_CACHE_VERSION,
            cache_builder_of_image_texture.value(),
            crc64_key);
    return nullptr;
}
