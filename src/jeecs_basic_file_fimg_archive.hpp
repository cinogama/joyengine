#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/file.hpp"

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
namespace jeecs::file::fimg
{
    // Comma/semicolon separated default list of already-compressed extensions.
    const char* default_skip_extensions();

    struct Entry
    {
        uint64_t shard;
        uint64_t offset;
        uint64_t stored_size; // bytes in the shard
        uint64_t size;        // logical (uncompressed) size
        uint32_t codec;
    };

    struct WriterOptions
    {
        int compression_level;        // 0 = store, 1-9 = deflate
        uint64_t min_compress_size;
        std::string skip_extensions;  // empty = no skipping
    };

    class Archive
    {
    public:
        // Returns nullptr when the manifest does not exist (a normal
        // development setup) or when it is malformed.
        static std::shared_ptr<const Archive> open(const std::string& root);

        const std::string& root() const
        {
            return m_root;
        }
        uint64_t shard_size() const
        {
            return m_shard_size;
        }

        const Entry* find(const std::string& path) const;
        std::string shard_path(uint64_t index) const;

    private:
        Archive(std::string root, uint64_t shard_size);

        std::string m_root;
        uint64_t m_shard_size;
        std::unordered_map<std::string, Entry> m_entries;
    };

    class File
    {
    public:
        File(std::shared_ptr<const Archive> owner, const Entry& ent);
        ~File();

        File(const File&) = delete;
        File& operator=(const File&) = delete;

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
        bool seek(int64_t offset, je_file_seek_mode_t origin);

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

        std::shared_ptr<const Archive> m_owner;
        Entry m_entry;
        uint64_t m_position;

        uint64_t m_open_shard;
        uint64_t m_open_shard_pos;
        je_raw_file_t m_handle;
        bool m_failed;

        uint64_t m_stored_cursor;
        uint64_t m_stream_position;
        bool m_stream_ended;
        std::unique_ptr<mz_stream_s> m_inflate_stream;
        bool m_inflate_initialized;
        std::vector<uint8_t> m_input_buffer;
        std::vector<uint8_t> m_skip_buffer;
    };

    class Writer
    {
    public:
        static std::unique_ptr<Writer> create(
            std::string root, uint64_t shard_size, WriterOptions options);
        ~Writer();

        Writer(const Writer&) = delete;
        Writer& operator=(const Writer&) = delete;

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
        Writer(std::string root, uint64_t shard_size, WriterOptions options);

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
        uint64_t m_shard_count;
        uint64_t m_shard_offset;
        FILE* m_shard_file;
        std::map<std::string, Entry> m_entries;
        bool m_failed;
        bool m_finished;

        int m_compression_level;
        uint64_t m_min_compress_size;
        std::unordered_set<std::string> m_skip_extensions;
        std::unique_ptr<mz_stream_s> m_deflate_stream;
        bool m_deflate_active;
        uint64_t m_deflate_output_size;
        std::vector<uint8_t> m_compress_buffer;
    };
}
