#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_fimg_archive.hpp"

#include "wo.h"

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

    je_raw_file_t _je_file_open(const char* path, size_t* out_len)
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

    size_t _je_file_read(void* buffer, size_t elemsz, size_t elemcount, je_raw_file_t file)
    {
        if (_m_read_file_impl != nullptr)
            return _m_read_file_impl(buffer, elemsz, elemcount, file);

        return fread(buffer, elemsz, elemcount, static_cast<FILE*>(file));
    }

    size_t _je_file_tell(je_raw_file_t file)
    {
        if (_m_read_file_tell_impl != nullptr)
            return _m_read_file_tell_impl(file);

#if defined(_WIN32)
        return static_cast<size_t>(_ftelli64(static_cast<FILE*>(file)));
#else
        return static_cast<size_t>(ftello(static_cast<FILE*>(file)));
#endif
    }

    int _je_file_seek(je_raw_file_t file, int64_t offset, je_file_seek_mode_t mode)
    {
        if (_m_read_file_seek_impl != nullptr)
            return _m_read_file_seek_impl(file, offset, mode);

#if defined(_WIN32)
        return _fseeki64(static_cast<FILE*>(file), offset, static_cast<int>(mode));
#else
        return fseeko(static_cast<FILE*>(file), static_cast<off_t>(offset), static_cast<int>(mode));
#endif
    }

    int _je_file_close(je_raw_file_t file)
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

struct je_file_t
{
    je_raw_file_t m_native_file_handle = nullptr;
    std::unique_ptr<jeecs::fimg::file> m_image_file_handle;
    size_t m_file_length = 0;
};

struct je_fimg_archive_t
{
    std::shared_ptr<const jeecs::fimg::archive> impl;
};

struct je_fimg_writer_t
{
    std::unique_ptr<jeecs::fimg::writer> impl;
};

void je_file_native_operator_register(
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

void je_file_host_path_set(const char* path)
{
    assert(path != nullptr);
    std::lock_guard<std::mutex> lock(_g_path_mutex);
    _g_host_path = path;
}

void je_file_runtime_path_set(const char* path)
{
    assert(path != nullptr);
    std::lock_guard<std::mutex> lock(_g_path_mutex);
    _g_runtime_path = path;
}

const char* je_file_host_path_get()
{
    std::lock_guard<std::mutex> lock(_g_path_mutex);
    return _g_host_path.c_str();
}

const char* je_file_runtime_path_get()
{
    std::lock_guard<std::mutex> lock(_g_path_mutex);
    return _g_runtime_path.c_str();
}

/* OPTIONAL */ je_file_t* je_file_open(const char* path)
{
    assert(path != nullptr);
    if (path[0] == '\0')
        return nullptr;

    const std::string resolved_path = _resolve_path(path);

    size_t file_length = 0;
    je_raw_file_t native_handle = _je_file_open(resolved_path.c_str(), &file_length);
    if (native_handle != nullptr)
    {
        auto* file = new je_file_t;
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

    auto* file = new je_file_t;
    file->m_image_file_handle
        = std::make_unique<jeecs::fimg::file>(std::move(archive), *entry);
    file->m_file_length = static_cast<size_t>(entry->size);
    return file;
}

void je_file_close(je_file_t* file)
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

size_t je_file_read(
    void* out_buffer,
    size_t elem_size,
    size_t count,
    je_file_t* file)
{
    assert(file != nullptr);
    assert(out_buffer != nullptr || elem_size == 0 || count == 0);

    if (file->m_native_file_handle != nullptr)
        return _je_file_read(out_buffer, elem_size, count, file->m_native_file_handle);

    return file->m_image_file_handle->read(out_buffer, elem_size, count);
}

size_t je_file_tell(je_file_t* file)
{
    assert(file != nullptr);

    if (file->m_native_file_handle != nullptr)
        return _je_file_tell(file->m_native_file_handle);

    return static_cast<size_t>(file->m_image_file_handle->tell());
}

bool je_file_seek(je_file_t* file, int64_t offset, je_file_seek_mode_t mode)
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

size_t je_file_size(je_file_t* file)
{
    assert(file != nullptr);
    return file->m_file_length;
}

bool je_fimg_default_archive_set(const char* root)
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

je_fimg_archive_t* je_fimg_archive_open(const char* root)
{
    assert(root != nullptr);

    auto impl = jeecs::fimg::archive::open(root);
    if (impl == nullptr)
        return nullptr;

    auto* archive = new je_fimg_archive_t;
    archive->impl = std::move(impl);
    return archive;
}

void je_fimg_archive_close(je_fimg_archive_t* archive)
{
    assert(archive != nullptr);
    delete archive;
}

bool je_fimg_archive_has_file(const je_fimg_archive_t* archive, const char* path)
{
    assert(archive != nullptr && archive->impl != nullptr);
    assert(path != nullptr);

    return archive->impl->find(path) != nullptr;
}

size_t je_fimg_archive_file_size(const je_fimg_archive_t* archive, const char* path)
{
    assert(archive != nullptr && archive->impl != nullptr);
    assert(path != nullptr);

    const jeecs::fimg::entry* entry = archive->impl->find(path);
    return entry == nullptr ? 0 : static_cast<size_t>(entry->size);
}

je_fimg_writer_t* je_fimg_writer_create(
    const char* root,
    size_t shard_size,
    const je_fimg_writer_options_t* options)
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

    auto* writer = new je_fimg_writer_t;
    writer->impl = std::move(impl);
    return writer;
}

const char* je_fimg_default_skip_extensions()
{
    return jeecs::fimg::default_skip_extensions();
}

bool je_fimg_writer_pack_file(
    je_fimg_writer_t* writer,
    const char* source_path,
    const char* archive_path)
{
    assert(writer != nullptr && writer->impl != nullptr);
    assert(source_path != nullptr && archive_path != nullptr);

    return writer->impl->pack_file(source_path, archive_path);
}

bool je_fimg_writer_pack_buffer(
    je_fimg_writer_t* writer,
    const void* buffer,
    size_t len,
    const char* archive_path)
{
    assert(writer != nullptr && writer->impl != nullptr);
    assert(archive_path != nullptr);
    assert(buffer != nullptr || len == 0);

    return writer->impl->pack_buffer(buffer, len, archive_path);
}

bool je_fimg_writer_finish(je_fimg_writer_t* writer)
{
    assert(writer != nullptr && writer->impl != nullptr);
    return writer->impl->finish();
}

void je_fimg_writer_abort(je_fimg_writer_t* writer)
{
    assert(writer != nullptr && writer->impl != nullptr);

    writer->impl->abort();
    delete writer;
}

je_image_texture_color_space_t je_image_texture_color_space_get(const char* path)
{
    assert(path != nullptr);

    const std::string settings_path = std::string(path) + ".je4texture";

    // 属性是可选资源：源文件不存在时直接按默认值处理，不调用生成器
    //（否则每张没有属性的纹理都会在加载时打印生成失败日志）。
    je_file_t* const source = je_file_open(settings_path.c_str());
    if (source == nullptr)
        return JE_IMAGE_TEXTURE_COLOR_SPACE_SRGB;
    je_file_close(source);

    je_file_t* const cache =
        je_image_texture_cache_open_or_create(settings_path.c_str());

    // 生成器不可用或生成失败时同样回退到 sRGB。
    if (cache == nullptr)
        return JE_IMAGE_TEXTURE_COLOR_SPACE_SRGB;

    je_cache_binary_document_t root;
    int32_t color_space = (int32_t)JE_IMAGE_TEXTURE_COLOR_SPACE_SRGB;

    const bool valid = je_cache_binary_document_open(cache, &root)
        && je_cache_binary_document_index(
            &root,
            "color_space",
            JE_CACHE_BINARY_VALUE_INT32,
            &color_space);
    je_file_close(cache);

    if (!valid)
    {
        // A cache file that exists but cannot be parsed is corruption, not a
        // miss; report it instead of silently treating it as the default.
        jeecs::debug::logerr(
            "Invalid image texture property cache for '%s'.",
            settings_path.c_str());
        return JE_IMAGE_TEXTURE_COLOR_SPACE_SRGB;
    }

    return color_space == (int32_t)JE_IMAGE_TEXTURE_COLOR_SPACE_LINEAR
        ? JE_IMAGE_TEXTURE_COLOR_SPACE_LINEAR
        : JE_IMAGE_TEXTURE_COLOR_SPACE_SRGB;
}

WOORT_API woort_api wojeapi_image_texture_cache_ensure(void)
{
    const std::string path = woort_string(0);

    je_file_t* cache = je_image_texture_cache_open_or_create(path.c_str());
    if (cache != nullptr)
    {
        je_file_close(cache);
        return woort_ret_bool(true);
    }

    return woort_ret_bool(false);
}
