#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_cache_version.hpp"

#include <string>



// WARNING!!!!
// DO NOT USE SIZE_T!!!!

wo_integer_t _crc64_of_file(const char* filepath)
{
    wo_integer_t crc64_result = 0;
    if (auto* origin_file = jeecs_file_open(filepath))
    {
        constexpr size_t READ_COUNT_PER_GROUP = 256;

        char buf[READ_COUNT_PER_GROUP];

        size_t reserved_byte_len = origin_file->m_file_length;
        while (reserved_byte_len > 0)
        {
            size_t this_time_read_size = std::min(READ_COUNT_PER_GROUP, reserved_byte_len);
            reserved_byte_len -= this_time_read_size;

            jeecs_file_read(buf, sizeof(char), this_time_read_size, origin_file);

            for (size_t i = 0; i < this_time_read_size; ++i)
                crc64_result = wo_crc64_u8(buf[i], crc64_result);
        }
        jeecs_file_close(origin_file);
    }

    return crc64_result;
}

jeecs_file* jeecs_load_cache_file(const char* filepath, uint32_t format_version, bool ignore_crc64)
{
    using namespace std;

    string file_cache_path = filepath + ".jecache4"s;
    // 1. Try open cache file for read.
    if (auto* cache_file = jeecs_file_open(file_cache_path.c_str()))
    {
        // 1. Verify format_version;
        uint32_t cache_mgr_ver, cache_format_ver;

        jeecs_file_read(&cache_mgr_ver, sizeof(uint32_t), 1, cache_file);
        if (cache_mgr_ver != cache_mgr_version)
        {
            jeecs::debug::logwarn("Found cache file when loading '%s', but cache's MGR_VERSION din't match.",
                filepath);
            jeecs_file_close(cache_file);
            return nullptr;
        }

        jeecs_file_read(&cache_format_ver, sizeof(uint32_t), 1, cache_file);
        if (cache_format_ver != format_version)
        {
            jeecs::debug::logwarn("Found cache file when loading '%s', but cache's FORMAT_VERSION din't match.",
                filepath);
            jeecs_file_close(cache_file);
            return nullptr;
        }

        // 2. Check crc64 of current file;
        wo_integer_t crc64_result = ignore_crc64 ? 0 : _crc64_of_file(filepath);

        wo_integer_t cache_crc64 = 0;
        jeecs_file_read(&cache_crc64, sizeof(wo_integer_t), 1, cache_file);

        if (crc64_result != 0)
        {
            if (crc64_result != cache_crc64)
            {
                jeecs::debug::logwarn("Found cache file when loading '%s', but cache's CRC64 din't match.",
                    filepath);
                jeecs_file_close(cache_file);
                return nullptr;
            }
        }
        else
            jeecs::debug::logwarn("Found cache file when loading '%s', but origin file missing.", filepath);
        return cache_file;
    }
    return nullptr;
}

void* jeecs_create_cache_file(const char* filepath, uint32_t format_version)
{
    using namespace std;

    wo_integer_t crc64_result = _crc64_of_file(filepath);
    if (crc64_result == 0)
    {
        jeecs::debug::logwarn("Empty or failed to read file: '%s'", filepath);
        return nullptr;
    }

    string file_cache_path = filepath + ".jecache4"s;
    FILE* f = fopen(file_cache_path.c_str(), "wb");

    if (f == nullptr)
    {
        jeecs::debug::logerr("Failed to create cache file for: '%s'", filepath);
        return nullptr;
    }

    fwrite(&cache_mgr_version, sizeof(uint32_t), 1, f);
    fwrite(&format_version, sizeof(uint32_t), 1, f);
    fwrite(&crc64_result, sizeof(wo_integer_t), 1, f);

    return f;
}

size_t jeecs_write_cache_file(const void* write_buffer, size_t elem_size, size_t count, void* file)
{
    return fwrite(write_buffer, elem_size, count, (FILE*)file);
}

void jeecs_close_cache_file(void* file)
{
    fclose((FILE*)file);
}