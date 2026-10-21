#include "jeecs_fimg_archive.hpp"

#include "wo.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>

namespace
{
    using namespace jeecs::fimg;

    constexpr uint8_t MANIFEST_MAGIC[8] = { 'J', 'E', '4', 'F', 'I', 'M', 'G', '\0' };
    constexpr size_t HEADER_SIZE = 56;
    constexpr size_t ENTRY_MIN_SIZE = 4 + 1 + 8 + 8 + 8; // path_len + path + shard + offset + size

    std::string join_path(const std::string& root, const std::string& name)
    {
        if (root.empty())
            return name;

        const char last = root.back();
        if (last == '/' || last == '\\')
            return root + name;

        return root + "/" + name;
    }

    std::string shard_file_name(uint64_t index)
    {
        return std::string(SHARD_FILE_PREFIX) + std::to_string(index) + SHARD_FILE_EXTENSION;
    }

    void put_u32(std::vector<uint8_t>& out, uint32_t value)
    {
        out.push_back(static_cast<uint8_t>(value & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    }

    void put_u64(std::vector<uint8_t>& out, uint64_t value)
    {
        for (int i = 0; i < 8; ++i)
            out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }

    bool read_u32(const uint8_t*& cursor, const uint8_t* end, uint32_t& value)
    {
        if (static_cast<size_t>(end - cursor) < 4)
            return false;

        value = static_cast<uint32_t>(cursor[0])
            | (static_cast<uint32_t>(cursor[1]) << 8)
            | (static_cast<uint32_t>(cursor[2]) << 16)
            | (static_cast<uint32_t>(cursor[3]) << 24);
        cursor += 4;
        return true;
    }

    bool read_u64(const uint8_t*& cursor, const uint8_t* end, uint64_t& value)
    {
        if (static_cast<size_t>(end - cursor) < 8)
            return false;

        value = 0;
        for (int i = 0; i < 8; ++i)
            value |= static_cast<uint64_t>(cursor[i]) << (i * 8);
        cursor += 8;
        return true;
    }

    uint64_t crc64(const uint8_t* data, size_t length)
    {
        uint64_t crc = 0;
        for (size_t i = 0; i < length; ++i)
            crc = wo_crc64_u8(data[i], crc);
        return crc;
    }
}

namespace jeecs::fimg
{
    archive::archive(std::string root, uint64_t shard_size)
        : m_root(std::move(root))
        , m_shard_size(shard_size)
    {
    }

    std::shared_ptr<const archive> archive::open(const std::string& root)
    {
        const std::string manifest_path = join_path(root, MANIFEST_FILE_NAME);

        size_t manifest_size = 0;
        io::raw_file handle = io::open(manifest_path.c_str(), &manifest_size);
        if (handle == nullptr)
        {
            // A missing manifest is a normal development setup: resources are
            // loaded from loose files.
            return nullptr;
        }

        if (manifest_size < HEADER_SIZE)
        {
            io::close(handle);
            report_error("fimg manifest '" + manifest_path + "' is truncated.");
            return nullptr;
        }
        if (manifest_size > MAX_MANIFEST_SIZE)
        {
            io::close(handle);
            report_error("fimg manifest '" + manifest_path + "' is too large.");
            return nullptr;
        }

        std::vector<uint8_t> data(manifest_size);
        size_t read_size = 0;
        while (read_size < manifest_size)
        {
            const size_t this_time = io::read(
                data.data() + read_size, 1, manifest_size - read_size, handle);

            if (this_time == 0)
                break;

            read_size += this_time;
        }
        io::close(handle);

        if (read_size != manifest_size)
        {
            report_error("failed to read fimg manifest '" + manifest_path + "'.");
            return nullptr;
        }

        const uint8_t* cursor = data.data();
        const uint8_t* const end = cursor + data.size();

        if (std::memcmp(cursor, MANIFEST_MAGIC, sizeof(MANIFEST_MAGIC)) != 0)
        {
            report_error("fimg manifest '" + manifest_path + "' has an invalid magic.");
            return nullptr;
        }
        cursor += sizeof(MANIFEST_MAGIC);

        uint32_t version = 0;
        uint32_t flags = 0;
        uint64_t shard_size = 0;
        uint64_t file_count = 0;
        uint64_t shard_count = 0;
        uint64_t total_size = 0;
        uint64_t index_crc64 = 0;

        if (!read_u32(cursor, end, version)
            || !read_u32(cursor, end, flags)
            || !read_u64(cursor, end, shard_size)
            || !read_u64(cursor, end, file_count)
            || !read_u64(cursor, end, shard_count)
            || !read_u64(cursor, end, total_size)
            || !read_u64(cursor, end, index_crc64))
        {
            report_error("fimg manifest '" + manifest_path + "' has a truncated header.");
            return nullptr;
        }

        if (version != FORMAT_VERSION)
        {
            report_error("fimg manifest '" + manifest_path
                + "' uses an unsupported format version ("
                + std::to_string(version) + ").");
            return nullptr;
        }
        if (flags != 0)
        {
            report_error("fimg manifest '" + manifest_path + "' uses unknown flags.");
            return nullptr;
        }
        if (shard_size == 0 || shard_size > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        {
            report_error("fimg manifest '" + manifest_path + "' has an invalid shard size.");
            return nullptr;
        }
        if (file_count > MAX_FILE_COUNT)
        {
            report_error("fimg manifest '" + manifest_path + "' declares too many files.");
            return nullptr;
        }

        const uint8_t* const entries_begin = cursor;
        if (file_count > static_cast<uint64_t>(end - entries_begin) / ENTRY_MIN_SIZE)
        {
            report_error("fimg manifest '" + manifest_path + "' has an invalid file count.");
            return nullptr;
        }
        if (crc64(entries_begin, static_cast<size_t>(end - entries_begin)) != index_crc64)
        {
            report_error("fimg manifest '" + manifest_path + "' has a broken index (crc mismatch).");
            return nullptr;
        }

        auto result = std::shared_ptr<archive>(new archive(root, shard_size));
        result->m_entries.reserve(static_cast<size_t>(file_count));

        uint64_t sum_size = 0;
        for (uint64_t i = 0; i < file_count; ++i)
        {
            uint32_t path_length = 0;
            if (!read_u32(cursor, end, path_length))
            {
                report_error("fimg manifest '" + manifest_path + "' has a truncated entry.");
                return nullptr;
            }

            if (path_length == 0
                || static_cast<uint64_t>(end - cursor) < static_cast<uint64_t>(path_length) + 24)
            {
                report_error("fimg manifest '" + manifest_path + "' has an invalid entry path.");
                return nullptr;
            }
            if (std::memchr(cursor, 0, path_length) != nullptr)
            {
                report_error("fimg manifest '" + manifest_path + "' contains a NUL in a path.");
                return nullptr;
            }

            std::string entry_path(reinterpret_cast<const char*>(cursor), path_length);
            cursor += path_length;

            entry ent;
            if (!read_u64(cursor, end, ent.shard)
                || !read_u64(cursor, end, ent.offset)
                || !read_u64(cursor, end, ent.size))
            {
                report_error("fimg manifest '" + manifest_path + "' has a truncated entry.");
                return nullptr;
            }

            if (ent.size != 0)
            {
                if (ent.offset >= shard_size)
                {
                    report_error("fimg manifest '" + manifest_path
                        + "' has an out-of-range offset for '" + entry_path + "'.");
                    return nullptr;
                }

                const uint64_t last_byte_in_shard = ent.size - 1;
                if (ent.offset > std::numeric_limits<uint64_t>::max() - last_byte_in_shard)
                {
                    report_error("fimg manifest '" + manifest_path
                        + "' has an overflowing size for '" + entry_path + "'.");
                    return nullptr;
                }

                const uint64_t shard_delta = (ent.offset + last_byte_in_shard) / shard_size;
                if (ent.shard > std::numeric_limits<uint64_t>::max() - shard_delta
                    || ent.shard + shard_delta >= shard_count)
                {
                    report_error("fimg manifest '" + manifest_path
                        + "' references a missing shard for '" + entry_path + "'.");
                    return nullptr;
                }
            }
            else if (ent.shard > shard_count)
            {
                report_error("fimg manifest '" + manifest_path
                    + "' has an invalid empty-file entry '" + entry_path + "'.");
                return nullptr;
            }

            if constexpr (sizeof(size_t) < sizeof(uint64_t))
            {
                if (ent.size > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
                {
                    report_error("fimg manifest '" + manifest_path
                        + "' contains a file too large for this platform: '" + entry_path + "'.");
                    return nullptr;
                }
            }

            if (sum_size > std::numeric_limits<uint64_t>::max() - ent.size)
            {
                report_error("fimg manifest '" + manifest_path + "' has an overflowing total size.");
                return nullptr;
            }
            sum_size += ent.size;

            auto insert_result = result->m_entries.emplace(std::move(entry_path), ent);
            if (!insert_result.second)
                report_warning("fimg manifest '" + manifest_path
                    + "' contains a duplicate path, keeping the first entry: '"
                    + insert_result.first->first + "'.");
        }

        if (cursor != end)
        {
            report_error("fimg manifest '" + manifest_path + "' has trailing garbage.");
            return nullptr;
        }
        if (sum_size != total_size)
        {
            report_error("fimg manifest '" + manifest_path + "' has an inconsistent total size.");
            return nullptr;
        }

        return result;
    }

    const entry* archive::find(const std::string& path) const
    {
        const auto iterator = m_entries.find(path);
        return iterator == m_entries.end() ? nullptr : &iterator->second;
    }

    std::string archive::shard_path(uint64_t index) const
    {
        return join_path(m_root, shard_file_name(index));
    }

    file::file(std::shared_ptr<const archive> owner, const entry& ent)
        : m_owner(std::move(owner))
        , m_entry(ent)
    {
        assert(m_owner != nullptr);
    }

    file::~file()
    {
        close_shard();
    }

    void file::close_shard()
    {
        if (m_handle != nullptr)
        {
            io::close(m_handle);
            m_handle = nullptr;
        }
        m_open_shard_pos = 0;
        m_open_shard = 0;
    }

    bool file::ensure_shard(uint64_t shard, uint64_t offset_in_shard)
    {
        if (m_failed)
            return false;

        if (m_handle != nullptr && m_open_shard == shard)
        {
            if (m_open_shard_pos != offset_in_shard)
            {
                if (io::seek(m_handle, static_cast<int64_t>(offset_in_shard), io::origin::begin) != 0)
                {
                    report_error("failed to seek inside fimg shard '" + m_owner->shard_path(shard) + "'.");
                    m_failed = true;
                    return false;
                }
                m_open_shard_pos = offset_in_shard;
            }
            return true;
        }

        close_shard();

        const std::string path = m_owner->shard_path(shard);
        size_t shard_size = 0;
        m_handle = io::open(path.c_str(), &shard_size);
        if (m_handle == nullptr)
        {
            report_error("failed to open fimg shard '" + path + "'.");
            m_failed = true;
            return false;
        }

        m_open_shard = shard;
        m_open_shard_pos = 0;

        if (offset_in_shard != 0)
        {
            if (io::seek(m_handle, static_cast<int64_t>(offset_in_shard), io::origin::begin) != 0)
            {
                report_error("failed to seek inside fimg shard '" + path + "'.");
                m_failed = true;
                return false;
            }
            m_open_shard_pos = offset_in_shard;
        }

        return true;
    }

    bool file::seek(int64_t offset, io::origin origin)
    {
        uint64_t base = 0;
        switch (origin)
        {
        case io::origin::begin:
            base = 0;
            break;
        case io::origin::current:
            base = m_position;
            break;
        case io::origin::end:
            base = m_entry.size;
            break;
        }

        uint64_t target = 0;
        if (offset >= 0)
        {
            const uint64_t delta = static_cast<uint64_t>(offset);
            target = base > std::numeric_limits<uint64_t>::max() - delta
                ? std::numeric_limits<uint64_t>::max()
                : base + delta;
        }
        else
        {
            const uint64_t magnitude = static_cast<uint64_t>(-(offset + 1)) + 1;
            target = base >= magnitude ? base - magnitude : 0;
        }

        m_position = std::min(target, m_entry.size);
        return true;
    }

    size_t file::read(void* buffer, size_t elem_size, size_t count)
    {
        assert(buffer != nullptr || elem_size == 0 || count == 0);

        if (m_failed || elem_size == 0 || count == 0)
            return 0;

        const uint64_t shard_size = m_owner->shard_size();
        uint64_t requested_size = 0;
        if (count > std::numeric_limits<uint64_t>::max() / elem_size)
            requested_size = std::numeric_limits<uint64_t>::max();
        else
            requested_size = static_cast<uint64_t>(elem_size) * static_cast<uint64_t>(count);

        const uint64_t remaining = m_entry.size - m_position;
        const uint64_t to_read = std::min(requested_size, remaining);

        auto* out = static_cast<uint8_t*>(buffer);
        uint64_t done = 0;
        while (done < to_read)
        {
            const uint64_t absolute = m_entry.offset + m_position;
            const uint64_t shard = m_entry.shard + absolute / shard_size;
            const uint64_t offset_in_shard = absolute % shard_size;
            const uint64_t chunk = std::min(to_read - done, shard_size - offset_in_shard);

            if (!ensure_shard(shard, offset_in_shard))
                break;

            const size_t this_time = io::read(out + done, 1, static_cast<size_t>(chunk), m_handle);
            if (this_time < chunk)
            {
                report_error("truncated fimg shard '" + m_owner->shard_path(shard)
                    + "' while reading the archive.");
                m_failed = true;
                done += this_time;
                m_position += this_time;
                m_open_shard_pos += this_time;
                break;
            }

            done += this_time;
            m_position += this_time;
            m_open_shard_pos += this_time;
        }

        return static_cast<size_t>(done / elem_size);
    }

    writer::writer(std::string root, uint64_t shard_size)
        : m_root(std::move(root))
        , m_shard_size(shard_size)
    {
    }

    writer::~writer()
    {
        close_current_shard();
    }

    std::unique_ptr<writer> writer::create(std::string root, uint64_t shard_size)
    {
        if (shard_size == 0)
            shard_size = DEFAULT_SHARD_SIZE;

        return std::unique_ptr<writer>(new writer(std::move(root), shard_size));
    }

    bool writer::open_next_shard()
    {
        if (m_shard_file != nullptr)
            return true;

        const std::string path = join_path(m_root, shard_file_name(m_shard_count));
        FILE* const file = std::fopen(path.c_str(), "wb");
        if (file == nullptr)
        {
            report_error("failed to create fimg shard '" + path + "'.");
            m_failed = true;
            return false;
        }

        m_shard_file = file;
        m_shard_offset = 0;
        return true;
    }

    bool writer::close_current_shard()
    {
        if (m_shard_file == nullptr)
            return true;

        const bool closed = std::fclose(m_shard_file) == 0;
        m_shard_file = nullptr;

        if (!closed)
        {
            report_error("failed to close the current fimg shard.");
            m_failed = true;
            return false;
        }

        ++m_shard_count;
        m_shard_offset = 0;
        return true;
    }

    bool writer::append(const void* data, size_t len)
    {
        const auto* bytes = static_cast<const uint8_t*>(data);
        while (len != 0)
        {
            if (m_failed)
                return false;

            if (!open_next_shard())
                return false;

            const uint64_t free_space = m_shard_size - m_shard_offset;
            const size_t write_size = static_cast<size_t>(
                std::min<uint64_t>(free_space, static_cast<uint64_t>(len)));

            if (std::fwrite(bytes, 1, write_size, m_shard_file) != write_size)
            {
                report_error("failed to write to the current fimg shard.");
                m_failed = true;
                return false;
            }

            bytes += write_size;
            len -= write_size;
            m_shard_offset += write_size;

            if (m_shard_offset == m_shard_size && !close_current_shard())
                return false;
        }

        return true;
    }

    bool writer::pack_buffer(const void* data, size_t len, std::string archive_path)
    {
        if (m_failed || m_finished)
            return false;
        if (archive_path.empty())
        {
            report_error("fimg writer received an empty archive path.");
            return false;
        }
        if (m_entries.size() >= MAX_FILE_COUNT)
        {
            report_error("fimg writer exceeded the maximum file count.");
            return false;
        }
        assert(data != nullptr || len == 0);
        if (m_entries.find(archive_path) != m_entries.end())
        {
            report_warning("duplicate fimg archive path ignored: '" + archive_path + "'.");
            return false;
        }

        entry ent;
        ent.shard = m_shard_count;
        ent.offset = m_shard_offset;
        ent.size = static_cast<uint64_t>(len);

        if (len != 0 && !append(data, len))
            return false;

        m_entries.emplace(std::move(archive_path), ent);
        return true;
    }

    bool writer::pack_file(const std::string& source_path, std::string archive_path)
    {
        if (m_failed || m_finished)
            return false;
        if (source_path.empty() || archive_path.empty())
        {
            report_error("fimg writer received an empty source or archive path.");
            return false;
        }
        if (m_entries.size() >= MAX_FILE_COUNT)
        {
            report_error("fimg writer exceeded the maximum file count.");
            return false;
        }
        if (m_entries.find(archive_path) != m_entries.end())
        {
            report_warning("duplicate fimg archive path ignored: '" + archive_path + "'.");
            return false;
        }

        size_t source_size = 0;
        io::raw_file source = io::open(source_path.c_str(), &source_size);
        if (source == nullptr)
        {
            report_error("failed to open source file for fimg packing: '" + source_path + "'.");
            return false;
        }

        entry ent;
        ent.shard = m_shard_count;
        ent.offset = m_shard_offset;
        ent.size = static_cast<uint64_t>(source_size);

        std::vector<uint8_t> buffer(64 * 1024);
        uint64_t copied = 0;
        bool succeeded = true;
        while (true)
        {
            const size_t this_time = io::read(buffer.data(), 1, buffer.size(), source);
            if (this_time == 0)
                break;

            if (!append(buffer.data(), this_time))
            {
                succeeded = false;
                break;
            }
            copied += this_time;
        }
        io::close(source);

        if (!succeeded)
            return false;

        if (copied != static_cast<uint64_t>(source_size))
        {
            report_error("source file changed while packing into fimg: '" + source_path + "'.");
            m_failed = true;
            return false;
        }

        m_entries.emplace(std::move(archive_path), ent);
        return true;
    }

    bool writer::write_manifest()
    {
        std::vector<uint8_t> entries_data;
        uint64_t total_size = 0;

        for (const auto& [path, ent] : m_entries)
        {
            if (path.size() > std::numeric_limits<uint32_t>::max())
            {
                report_error("fimg archive path is too long: '" + path + "'.");
                return false;
            }
            if (total_size > std::numeric_limits<uint64_t>::max() - ent.size)
            {
                report_error("fimg archive total size overflows.");
                return false;
            }
            total_size += ent.size;

            put_u32(entries_data, static_cast<uint32_t>(path.size()));
            entries_data.insert(entries_data.end(), path.begin(), path.end());
            put_u64(entries_data, ent.shard);
            put_u64(entries_data, ent.offset);
            put_u64(entries_data, ent.size);
        }

        std::vector<uint8_t> manifest;
        manifest.reserve(HEADER_SIZE + entries_data.size());
        manifest.insert(manifest.end(), std::begin(MANIFEST_MAGIC), std::end(MANIFEST_MAGIC));
        put_u32(manifest, FORMAT_VERSION);
        put_u32(manifest, 0);
        put_u64(manifest, m_shard_size);
        put_u64(manifest, static_cast<uint64_t>(m_entries.size()));
        put_u64(manifest, m_shard_count);
        put_u64(manifest, total_size);
        put_u64(manifest, crc64(entries_data.data(), entries_data.size()));
        manifest.insert(manifest.end(), entries_data.begin(), entries_data.end());

        const std::string manifest_path = join_path(m_root, MANIFEST_FILE_NAME);

        FILE* const file = std::fopen(manifest_path.c_str(), "wb");
        if (file == nullptr)
        {
            report_error("failed to create fimg manifest '" + manifest_path + "'.");
            return false;
        }

        bool written = std::fwrite(manifest.data(), 1, manifest.size(), file) == manifest.size()
            && std::fflush(file) == 0;
        if (std::fclose(file) != 0)
            written = false;

        if (!written)
        {
            report_error("failed to write fimg manifest '" + manifest_path + "'.");
            return false;
        }

        return true;
    }

    bool writer::finish()
    {
        if (m_finished)
            return true;
        if (m_failed)
            return false;

        if (!close_current_shard())
            return false;

        if (!write_manifest())
        {
            m_failed = true;
            return false;
        }

        m_finished = true;
        return true;
    }

    void writer::abort()
    {
        close_current_shard();
    }
}
