#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Internal fimg archive format (v2).
//
// An archive is a directory containing one manifest and N data shards:
//   fimg.je4index
//   fimg.0.je4image
//   fimg.1.je4image
//   ...
//
// This module only depends on the standard library. The platform file
// operations are supplied by the `io` facade, which is implemented by
// jeecs_basic_file_image.cpp (or by a test harness).
namespace jeecs::fimg
{
    inline constexpr const char* MANIFEST_FILE_NAME = "fimg.je4index";
    inline constexpr const char* SHARD_FILE_PREFIX = "fimg.";
    inline constexpr const char* SHARD_FILE_EXTENSION = ".je4image";

    inline constexpr uint32_t FORMAT_VERSION = 2;
    inline constexpr size_t DEFAULT_SHARD_SIZE = 1 * 1024 * 1024;
    inline constexpr size_t MAX_MANIFEST_SIZE = 64 * 1024 * 1024;
    inline constexpr uint64_t MAX_FILE_COUNT = 1 * 1024 * 1024;

    namespace io
    {
        using raw_file = void*;

        enum class origin
        {
            begin,
            current,
            end,
        };

        raw_file open(const char* path, size_t* out_size);
        size_t read(void* buffer, size_t elem_size, size_t count, raw_file file);
        size_t tell(raw_file file);
        int seek(raw_file file, int64_t offset, origin origin);
        int close(raw_file file);
    }

    void report_error(const std::string& message);
    void report_warning(const std::string& message);

    struct entry
    {
        uint64_t shard = 0;
        uint64_t offset = 0;
        uint64_t size = 0;
    };

    class archive
    {
    public:
        // Returns nullptr when the manifest does not exist (a normal
        // development setup) or when it is malformed.
        static std::shared_ptr<const archive> open(const std::string& root);

        const std::string& root() const
        {
            return m_root;
        }
        uint64_t shard_size() const
        {
            return m_shard_size;
        }

        const entry* find(const std::string& path) const;
        std::string shard_path(uint64_t index) const;

    private:
        archive(std::string root, uint64_t shard_size);

        std::string m_root;
        uint64_t m_shard_size;
        std::unordered_map<std::string, entry> m_entries;
    };

    class file
    {
    public:
        file(std::shared_ptr<const archive> owner, const entry& ent);
        ~file();

        file(const file&) = delete;
        file& operator=(const file&) = delete;

        uint64_t size() const
        {
            return m_entry.size;
        }
        uint64_t tell() const
        {
            return m_position;
        }
        bool failed() const
        {
            return m_failed;
        }

        // The resulting position is clamped to [0, size].
        bool seek(int64_t offset, io::origin origin);

        // Returns the number of complete elements read.
        size_t read(void* buffer, size_t elem_size, size_t count);

    private:
        bool ensure_shard(uint64_t shard, uint64_t offset_in_shard);
        void close_shard();

        std::shared_ptr<const archive> m_owner;
        entry m_entry;
        uint64_t m_position = 0;
        uint64_t m_open_shard = 0;
        uint64_t m_open_shard_pos = 0;
        io::raw_file m_handle = nullptr;
        bool m_failed = false;
    };

    class writer
    {
    public:
        static std::unique_ptr<writer> create(std::string root, uint64_t shard_size);
        ~writer();

        writer(const writer&) = delete;
        writer& operator=(const writer&) = delete;

        bool pack_buffer(const void* data, size_t len, std::string archive_path);
        bool pack_file(const std::string& source_path, std::string archive_path);
        bool finish();

        // Closes the writer. If finish() has not succeeded, already written
        // files are left on disk; the caller owns the output directory.
        void abort();

        bool failed() const
        {
            return m_failed;
        }

    private:
        writer(std::string root, uint64_t shard_size);

        bool append(const void* data, size_t len);
        bool open_next_shard();
        bool close_current_shard();
        bool write_manifest();

        std::string m_root;
        uint64_t m_shard_size;
        uint64_t m_shard_count = 0;
        uint64_t m_shard_offset = 0;
        FILE* m_shard_file = nullptr;
        std::map<std::string, entry> m_entries;
        bool m_failed = false;
        bool m_finished = false;
    };
}
