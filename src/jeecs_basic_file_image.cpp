#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_cache_version.hpp"
#include "jeecs_fimg_archive.hpp"

#include <cstdio>
#include <sys/stat.h>

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>

namespace
{
    je_read_file_open_func_t _m_read_file_open_impl = nullptr;
    je_read_file_func_t _m_read_file_impl = nullptr;
    je_read_file_tell_func_t _m_read_file_tell_impl = nullptr;
    je_read_file_seek_func_t _m_read_file_seek_impl = nullptr;
    je_read_file_close_func_t _m_read_file_close_impl = nullptr;

    jeecs_raw_file _je_file_open(const char* path, size_t* out_len)
    {
        if (_m_read_file_open_impl != nullptr)
            return _m_read_file_open_impl(path, out_len);

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
        return fhandle;
    }

    size_t _je_file_read(void* buffer, size_t elemsz, size_t elemcount, jeecs_raw_file file)
    {
        if (_m_read_file_impl != nullptr)
            return _m_read_file_impl(buffer, elemsz, elemcount, file);

        return fread(buffer, elemsz, elemcount, static_cast<FILE*>(file));
    }

    size_t _je_file_tell(jeecs_raw_file file)
    {
        if (_m_read_file_tell_impl != nullptr)
            return _m_read_file_tell_impl(file);

#if defined(_WIN32)
        return static_cast<size_t>(_ftelli64(static_cast<FILE*>(file)));
#else
        return static_cast<size_t>(ftello(static_cast<FILE*>(file)));
#endif
    }

    int _je_file_seek(jeecs_raw_file file, int64_t offset, je_read_file_seek_mode mode)
    {
        if (_m_read_file_seek_impl != nullptr)
            return _m_read_file_seek_impl(file, offset, mode);

#if defined(_WIN32)
        return _fseeki64(static_cast<FILE*>(file), offset, static_cast<int>(mode));
#else
        return fseeko(static_cast<FILE*>(file), static_cast<off_t>(offset), static_cast<int>(mode));
#endif
    }

    int _je_file_close(jeecs_raw_file file)
    {
        if (_m_read_file_close_impl != nullptr)
            return _m_read_file_close_impl(file);

        return fclose(static_cast<FILE*>(file));
    }

    std::mutex _g_path_mutex;
    std::string _g_host_path;
    std::string _g_runtime_path;

    std::shared_mutex _g_default_archive_mutex;
    std::shared_ptr<const jeecs::fimg::archive> _g_default_archive;

    std::string _get_host_path_copy()
    {
        std::lock_guard<std::mutex> lock(_g_path_mutex);
        return _g_host_path;
    }

    std::string _get_runtime_path_copy()
    {
        std::lock_guard<std::mutex> lock(_g_path_mutex);
        return _g_runtime_path;
    }

    std::string _join_root_and_rest(const std::string& root, const std::string& rest)
    {
        if (root.empty())
            return rest;
        if (rest.empty())
            return root;

        const bool root_has_separator = root.back() == '/' || root.back() == '\\';
        const bool rest_has_separator = rest.front() == '/' || rest.front() == '\\';

        if (root_has_separator && rest_has_separator)
            return root + rest.substr(1);
        if (!root_has_separator && !rest_has_separator)
            return root + "/" + rest;
        return root + rest;
    }

    std::string _resolve_path(const char* path)
    {
        if (path[0] == '@')
        {
            std::string root = _get_runtime_path_copy();
            if (root.empty())
                root = _get_host_path_copy();

            return _join_root_and_rest(root, path + 1);
        }

        if (path[0] == '!')
            return _join_root_and_rest(_get_host_path_copy(), path + 1);

        return path;
    }
}

namespace jeecs::fimg::io
{
    raw_file open(const char* path, size_t* out_size)
    {
        assert(path != nullptr && out_size != nullptr);
        return _je_file_open(path, out_size);
    }

    size_t read(void* buffer, size_t elem_size, size_t count, raw_file file)
    {
        assert(file != nullptr);
        assert(buffer != nullptr || elem_size == 0 || count == 0);
        return _je_file_read(buffer, elem_size, count, file);
    }

    size_t tell(raw_file file)
    {
        assert(file != nullptr);
        return _je_file_tell(file);
    }

    int seek(raw_file file, int64_t offset, origin origin)
    {
        assert(file != nullptr);
        switch (origin)
        {
        case origin::begin:
            return _je_file_seek(file, offset, JE_READ_FILE_SEEK_SET);
        case origin::current:
            return _je_file_seek(file, offset, JE_READ_FILE_SEEK_CURRENT);
        case origin::end:
            return _je_file_seek(file, offset, JE_READ_FILE_SEEK_END);
        }
        return -1;
    }

    int close(raw_file file)
    {
        assert(file != nullptr);
        return _je_file_close(file);
    }
}

namespace jeecs::fimg
{
    void report_error(const std::string& message)
    {
        jeecs::debug::logerr("%s", message.c_str());
    }

    void report_warning(const std::string& message)
    {
        jeecs::debug::logwarn("%s", message.c_str());
    }
}

struct jeecs_file
{
    jeecs_raw_file m_native_file_handle = nullptr;
    std::unique_ptr<jeecs::fimg::file> m_image_file_handle;
    size_t m_file_length = 0;
};

struct jeecs_fimg_archive
{
    std::shared_ptr<const jeecs::fimg::archive> impl;
};

struct jeecs_fimg_writer
{
    std::unique_ptr<jeecs::fimg::writer> impl;
};

void jeecs_register_native_file_operator(
    je_read_file_open_func_t opener,
    je_read_file_func_t reader,
    je_read_file_tell_func_t teller,
    je_read_file_seek_func_t seeker,
    je_read_file_close_func_t closer)
{
    assert(
        opener != nullptr && reader != nullptr && teller != nullptr && seeker != nullptr && closer != nullptr);
    _m_read_file_open_impl = opener;
    _m_read_file_impl = reader;
    _m_read_file_tell_impl = teller;
    _m_read_file_seek_impl = seeker;
    _m_read_file_close_impl = closer;
}

void jeecs_file_set_host_path(const char* path)
{
    assert(path != nullptr);
    std::lock_guard<std::mutex> lock(_g_path_mutex);
    _g_host_path = path;
}

void jeecs_file_set_runtime_path(const char* path)
{
    assert(path != nullptr);
    std::lock_guard<std::mutex> lock(_g_path_mutex);
    _g_runtime_path = path;
}

const char* jeecs_file_get_host_path()
{
    std::lock_guard<std::mutex> lock(_g_path_mutex);
    return _g_host_path.c_str();
}

const char* jeecs_file_get_runtime_path()
{
    std::lock_guard<std::mutex> lock(_g_path_mutex);
    return _g_runtime_path.c_str();
}

/* OPTIONAL */ jeecs_file* jeecs_file_open(const char* path)
{
    assert(path != nullptr);
    if (path[0] == '\0')
        return nullptr;

    const std::string resolved_path = _resolve_path(path);

    size_t file_length = 0;
    jeecs_raw_file native_handle = _je_file_open(resolved_path.c_str(), &file_length);
    if (native_handle != nullptr)
    {
        auto* file = new jeecs_file;
        file->m_native_file_handle = native_handle;
        file->m_file_length = file_length;
        return file;
    }

    if (path[0] != '@' && path[0] != '!')
        return nullptr;

    std::shared_ptr<const jeecs::fimg::archive> archive;
    {
        std::shared_lock<std::shared_mutex> lock(_g_default_archive_mutex);
        archive = _g_default_archive;
    }

    if (archive == nullptr)
        return nullptr;

    const jeecs::fimg::entry* entry = archive->find(path);
    if (entry == nullptr)
        return nullptr;

    auto* file = new jeecs_file;
    file->m_image_file_handle
        = std::make_unique<jeecs::fimg::file>(std::move(archive), *entry);
    file->m_file_length = static_cast<size_t>(entry->size);
    return file;
}

void jeecs_file_close(jeecs_file* file)
{
    assert(file != nullptr);

    if (file->m_native_file_handle != nullptr)
    {
        auto close_state = _je_file_close(file->m_native_file_handle);
        if (close_state != 0)
            jeecs::debug::logerr("Fail to close file(%d:%d).", (int)close_state, (int)errno);
    }

    file->m_image_file_handle.reset();
    delete file;
}

size_t jeecs_file_read(
    void* out_buffer,
    size_t elem_size,
    size_t count,
    jeecs_file* file)
{
    assert(file != nullptr);
    assert(out_buffer != nullptr || elem_size == 0 || count == 0);

    if (file->m_native_file_handle != nullptr)
        return _je_file_read(out_buffer, elem_size, count, file->m_native_file_handle);

    return file->m_image_file_handle->read(out_buffer, elem_size, count);
}

size_t jeecs_file_tell(jeecs_file* file)
{
    assert(file != nullptr);

    if (file->m_native_file_handle != nullptr)
        return _je_file_tell(file->m_native_file_handle);

    return static_cast<size_t>(file->m_image_file_handle->tell());
}

bool jeecs_file_seek(jeecs_file* file, int64_t offset, je_read_file_seek_mode mode)
{
    assert(file != nullptr);

    if (file->m_native_file_handle != nullptr)
        return _je_file_seek(file->m_native_file_handle, offset, mode) == 0;

    switch (mode)
    {
    case JE_READ_FILE_SEEK_SET:
        return file->m_image_file_handle->seek(offset, jeecs::fimg::io::origin::begin);
    case JE_READ_FILE_SEEK_CURRENT:
        return file->m_image_file_handle->seek(offset, jeecs::fimg::io::origin::current);
    case JE_READ_FILE_SEEK_END:
        return file->m_image_file_handle->seek(offset, jeecs::fimg::io::origin::end);
    }

    return false;
}

size_t jeecs_file_size(jeecs_file* file)
{
    assert(file != nullptr);
    return file->m_file_length;
}

bool jeecs_fimg_set_default_archive(const char* root)
{
    std::shared_ptr<const jeecs::fimg::archive> archive;
    if (root != nullptr)
        archive = jeecs::fimg::archive::open(root);

    const bool loaded = root == nullptr || archive != nullptr;

    std::shared_ptr<const jeecs::fimg::archive> old_archive;
    {
        std::unique_lock<std::shared_mutex> lock(_g_default_archive_mutex);
        old_archive = std::move(_g_default_archive);
        _g_default_archive = std::move(archive);
    }

    return loaded;
}

jeecs_fimg_archive* jeecs_fimg_archive_open(const char* root)
{
    assert(root != nullptr);

    auto impl = jeecs::fimg::archive::open(root);
    if (impl == nullptr)
        return nullptr;

    auto* archive = new jeecs_fimg_archive;
    archive->impl = std::move(impl);
    return archive;
}

void jeecs_fimg_archive_close(jeecs_fimg_archive* archive)
{
    assert(archive != nullptr);
    delete archive;
}

bool jeecs_fimg_archive_has_file(const jeecs_fimg_archive* archive, const char* path)
{
    assert(archive != nullptr && archive->impl != nullptr);
    assert(path != nullptr);

    return archive->impl->find(path) != nullptr;
}

size_t jeecs_fimg_archive_file_size(const jeecs_fimg_archive* archive, const char* path)
{
    assert(archive != nullptr && archive->impl != nullptr);
    assert(path != nullptr);

    const jeecs::fimg::entry* entry = archive->impl->find(path);
    return entry == nullptr ? 0 : static_cast<size_t>(entry->size);
}

jeecs_fimg_writer* jeecs_fimg_writer_create(
    const char* root,
    size_t shard_size,
    const jeecs_fimg_writer_options* options)
{
    assert(root != nullptr);

    jeecs::fimg::writer_options resolved;
    if (options != nullptr)
    {
        resolved.compression_level = options->compression_level;
        resolved.min_compress_size = options->min_compress_size;
        resolved.skip_extensions = options->skip_extensions != nullptr
            ? options->skip_extensions
            : jeecs::fimg::default_skip_extensions();
    }
    else
    {
        resolved.compression_level = 6;
        resolved.min_compress_size = 64;
        resolved.skip_extensions = jeecs::fimg::default_skip_extensions();
    }

    auto impl = jeecs::fimg::writer::create(root, shard_size, std::move(resolved));

    auto* writer = new jeecs_fimg_writer;
    writer->impl = std::move(impl);
    return writer;
}

const char* jeecs_fimg_default_skip_extensions()
{
    return jeecs::fimg::default_skip_extensions();
}

bool jeecs_fimg_writer_pack_file(
    jeecs_fimg_writer* writer,
    const char* source_path,
    const char* archive_path)
{
    assert(writer != nullptr && writer->impl != nullptr);
    assert(source_path != nullptr && archive_path != nullptr);

    return writer->impl->pack_file(source_path, archive_path);
}

bool jeecs_fimg_writer_pack_buffer(
    jeecs_fimg_writer* writer,
    const void* buffer,
    size_t len,
    const char* archive_path)
{
    assert(writer != nullptr && writer->impl != nullptr);
    assert(archive_path != nullptr);
    assert(buffer != nullptr || len == 0);

    return writer->impl->pack_buffer(buffer, len, archive_path);
}

bool jeecs_fimg_writer_finish(jeecs_fimg_writer* writer)
{
    assert(writer != nullptr && writer->impl != nullptr);
    return writer->impl->finish();
}

void jeecs_fimg_writer_abort(jeecs_fimg_writer* writer)
{
    assert(writer != nullptr && writer->impl != nullptr);

    writer->impl->abort();
    delete writer;
}

je_image_texture_color_space je_image_texture_get_color_space(const char* path)
{
    assert(path != nullptr);

    // Missing/stale caches intentionally fall back to sRGB, matching the
    // historical behavior of image textures before this property existed.
    if (auto* cache = jeecs_load_cache_file(path, IMAGE_TEXTURE_CACHE_VERSION, 0))
    {
        uint32_t color_space = (uint32_t)JE_IMAGE_TEXTURE_COLOR_SPACE_SRGB;
        const size_t read = jeecs_file_read(&color_space, sizeof(color_space), 1, cache);
        jeecs_file_close(cache);

        if (read == 1 && color_space == (uint32_t)JE_IMAGE_TEXTURE_COLOR_SPACE_LINEAR)
            return JE_IMAGE_TEXTURE_COLOR_SPACE_LINEAR;
    }
    return JE_IMAGE_TEXTURE_COLOR_SPACE_SRGB;
}

bool je_image_texture_set_color_space(
    const char* path, je_image_texture_color_space color_space)
{
    assert(path != nullptr);

    // jeecs_create_cache_file also embeds the image CRC64, so changing the
    // image later invalidates this property cache automatically.
    void* cache = jeecs_create_cache_file(path, IMAGE_TEXTURE_CACHE_VERSION, 0);
    if (cache == nullptr)
        return false;

    const uint32_t value =
        color_space == JE_IMAGE_TEXTURE_COLOR_SPACE_LINEAR ? 1u : 0u;
    const size_t written =
        jeecs_write_cache_file(&value, sizeof(value), 1, cache);
    jeecs_close_cache_file(cache);
    return written == 1;
}
