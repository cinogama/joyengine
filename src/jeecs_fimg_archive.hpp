#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct mz_stream_s;

// Internal fimg archive format (v3).
//
// An archive is a directory containing one manifest and N data shards:
//   fimg.je4index
//   fimg.0.je4image
//   fimg.1.je4image
//   ...
//
// Every entry is either stored verbatim or compressed as an independent raw
// deflate stream. This module only depends on the standard library, miniz and
// the platform file operations supplied by the `io` facade.
namespace jeecs::fimg
{
    inline constexpr const char* MANIFEST_FILE_NAME = "fimg.je4index";
    inline constexpr const char* SHARD_FILE_PREFIX = "fimg.";
    inline constexpr const char* SHARD_FILE_EXTENSION = ".je4image";

    inline constexpr uint32_t FORMAT_VERSION = 3;

    inline constexpr uint32_t CODEC_STORE = 0;
    inline constexpr uint32_t CODEC_DEFLATE = 1;

    inline constexpr size_t DEFAULT_SHARD_SIZE = 1 * 1024 * 1024;
    inline constexpr size_t MAX_MANIFEST_SIZE = 64 * 1024 * 1024;
    inline constexpr uint64_t MAX_FILE_COUNT = 1 * 1024 * 1024;
    inline constexpr size_t COMPRESSION_BUFFER_SIZE = 64 * 1024;

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

    // Comma/semicolon separated default list of already-compressed extensions.
    const char* default_skip_extensions();

    struct entry
    {
        uint64_t shard = 0;
        uint64_t offset = 0;
        uint64_t stored_size = 0; // bytes in the shard
        uint64_t size = 0;        // logical (uncompressed) size
        uint32_t codec = CODEC_STORE;
    };

    struct writer_options
    {
        int compression_level = 6;      // 0 = store, 1-9 = deflate
        uint64_t min_compress_size = 64;
        std::string skip_extensions;    // empty = no skipping
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

        size_t read_stored(void* buffer, size_t elem_size, size_t count);
        size_t read_compressed(void* buffer, size_t elem_size, size_t count);

        bool read_stored_bytes(void* buffer, size_t len);
        bool ensure_inflater();
        void reset_inflater();
        void end_inflater();
        bool fill_input();
        bool inflate_some(void* out, size_t capacity, size_t& produced);
        bool skip_to_position();

        std::shared_ptr<const archive> m_owner;
        entry m_entry;
        uint64_t m_position = 0;

        uint64_t m_open_shard = 0;
        uint64_t m_open_shard_pos = 0;
        io::raw_file m_handle = nullptr;
        bool m_failed = false;

        uint64_t m_stored_cursor = 0;
        uint64_t m_stream_position = 0;
        bool m_stream_ended = false;
        std::unique_ptr<mz_stream_s> m_inflate_stream;
        bool m_inflate_initialized = false;
        std::vector<uint8_t> m_input_buffer;
        std::vector<uint8_t> m_skip_buffer;
    };

    class writer
    {
    public:
        static std::unique_ptr<writer> create(
            std::string root, uint64_t shard_size, writer_options options);
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
        writer(std::string root, uint64_t shard_size, writer_options options);

        bool append(const void* data, size_t len);
        bool open_next_shard();
        bool close_current_shard();
        bool write_manifest();

        uint32_t select_codec(const std::string& archive_path, uint64_t size) const;
        bool begin_deflate();
        bool deflate_append(const void* data, size_t len);
        bool finish_deflate(uint64_t& stored_size);
        void end_deflate();

        std::string m_root;
        uint64_t m_shard_size;
        uint64_t m_shard_count = 0;
        uint64_t m_shard_offset = 0;
        FILE* m_shard_file = nullptr;
        std::map<std::string, entry> m_entries;
        bool m_failed = false;
        bool m_finished = false;

        int m_compression_level;
        uint64_t m_min_compress_size;
        std::unordered_set<std::string> m_skip_extensions;
        std::unique_ptr<mz_stream_s> m_deflate_stream;
        bool m_deflate_active = false;
        uint64_t m_deflate_output_size = 0;
        std::vector<uint8_t> m_compress_buffer;
    };
}
