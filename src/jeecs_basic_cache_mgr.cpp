#define JE_IMPL
#include "jeecs/cache.hpp"
#include "jeecs/file.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs_basic_cache_version.hpp"

#include "wo.h"

#include <cstdio>
#include <string>

// WARNING!!!!
// DO NOT USE SIZE_T!!!!

uint64_t je_file_crc64(const char* filepath, uint64_t crc64_continue)
{
    uint64_t crc64_result = crc64_continue;
    if (auto* origin_file = je_file_open(filepath))
    {
        constexpr size_t READ_COUNT_PER_GROUP = 256;

        char buf[READ_COUNT_PER_GROUP];

        size_t reserved_byte_len = je_file_size(origin_file);
        while (reserved_byte_len > 0)
        {
            size_t this_time_read_size = std::min(READ_COUNT_PER_GROUP, reserved_byte_len);
            reserved_byte_len -= this_time_read_size;

            je_file_read(buf, sizeof(char), this_time_read_size, origin_file);

            for (size_t i = 0; i < this_time_read_size; ++i)
                crc64_result = wo_crc64_u8(buf[i], crc64_result);
        }
        je_file_close(origin_file);
    }

    return crc64_result;
}

/* OPTIONAL */ je_file_t* je_cache_file_load(
    const char* filepath, uint32_t format_version, uint64_t virtual_crc64)
{
    using namespace std;

    string file_cache_path = filepath + ".je4cache"s;
    // 1. Try open cache file for read.
    if (auto* cache_file = je_file_open(file_cache_path.c_str()))
    {
        // 1. Verify format_version;
        uint32_t cache_mgr_ver, cache_format_ver;

        je_file_read(&cache_mgr_ver, sizeof(uint32_t), 1, cache_file);
        if (cache_mgr_ver != CACHE_MANAGER_MAIN_VERSION || CACHE_MANAGER_MAIN_VERSION == 0)
        {
            jeecs::debug::loginfo(
                "Reloading `%s`, cache file outdated (cache-manager version changed).",
                filepath);

            je_file_close(cache_file);
            return nullptr;
        }

        je_file_read(&cache_format_ver, sizeof(uint32_t), 1, cache_file);
        if (cache_format_ver != format_version)
        {
            jeecs::debug::loginfo(
                "Reloading `%s`, cache file outdated (cache version changed).",
                filepath);

            je_file_close(cache_file);
            return nullptr;
        }

        // 2. Check crc64 of current file;
        if (virtual_crc64 == ~(uint64_t)0)
        {
            // Origin file missing, it's Okay.
        }
        else
        {
            uint64_t crc64_result =
                virtual_crc64 ? virtual_crc64 : je_file_crc64(filepath, 0);

            uint64_t cache_crc64 = 0;
            je_file_read(&cache_crc64, sizeof(uint64_t), 1, cache_file);

            if (crc64_result != 0)
            {
                if (crc64_result != cache_crc64)
                {
                    jeecs::debug::loginfo(
                        "Reloading `%s`, cache file outdated (origin file changed).",
                        filepath);
                    je_file_close(cache_file);
                    return nullptr;
                }
            }
        }
        return cache_file;
    }
    return nullptr;
}

/* OPTIONAL */ je_writing_cache_file_t* je_cache_file_create(
    const char* filepath, uint32_t format_version, uint64_t usecrc64)
{
    using namespace std;

    uint64_t crc64_result = usecrc64 == 0 ? je_file_crc64(filepath, 0) : usecrc64;
    if (crc64_result == 0)
    {
        jeecs::debug::logwarn(
            "Empty or failed to read file: '%s', failed to get crc64, create cache failed.", 
            filepath);

        return nullptr;
    }

    string file_cache_path = filepath + ".je4cache"s;
    if (file_cache_path[0] == '@')
        file_cache_path = je_file_runtime_path_get() + file_cache_path.substr(1);
    if (file_cache_path[0] == '!')
        file_cache_path = je_file_host_path_get() + file_cache_path.substr(1);

    FILE* f = fopen(file_cache_path.c_str(), "wb");

    if (f == nullptr)
    {
        jeecs::debug::logwarn("Failed to create cache file for: '%s'", filepath);
        return nullptr;
    }

    // The main version is left as an invalid placeholder; the cache file can
    // never pass je_cache_file_load's validation until je_cache_file_close
    // writes it back with finished == true.
    constexpr uint32_t UNFINISHED_MAIN_VERSION_PLACEHOLDER = 0;
    fwrite(&UNFINISHED_MAIN_VERSION_PLACEHOLDER, sizeof(uint32_t), 1, f);
    fwrite(&format_version, sizeof(uint32_t), 1, f);
    fwrite(&crc64_result, sizeof(uint64_t), 1, f);

    return reinterpret_cast<je_writing_cache_file_t*>(f);
}

size_t je_cache_file_write(
    const void* write_buffer, 
    size_t elem_size, 
    size_t count, 
    je_writing_cache_file_t* file)
{
    return fwrite(write_buffer, elem_size, count, reinterpret_cast<FILE*>(file));
}

void je_cache_file_close(je_writing_cache_file_t* file, bool finished)
{
    FILE* const f = reinterpret_cast<FILE*>(file);

    if (finished)
    {
        // Write back the main version to make this cache file take effect.
        fseek(f, 0, SEEK_SET);
        fwrite(&CACHE_MANAGER_MAIN_VERSION, sizeof(uint32_t), 1, f);
    }

    fclose(f);
}
