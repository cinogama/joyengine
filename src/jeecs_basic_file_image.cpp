#define JE_IMPL
#include "jeecs/file.hpp"
#include "jeecs/logging.hpp"
#include "jeecs_basic_file_fimg_archive.hpp"
#include "jeecs_basic_file.hpp"

#include <cstdio>
#include <sys/stat.h>

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>

namespace jeecs::file {}

using namespace std;
using namespace jeecs;
using namespace jeecs::file;

namespace jeecs::file
{
    namespace
    {
        mutex _g_path_mutex;
        string _g_host_path;
        string _g_runtime_path;

        shared_mutex _g_default_archive_mutex;
        shared_ptr<const fimg::Archive> _g_default_archive;

        string _get_host_path_copy()
        {
            lock_guard<mutex> lock(_g_path_mutex);
            return _g_host_path;
        }
        string _get_runtime_path_copy()
        {
            lock_guard<mutex> lock(_g_path_mutex);
            return _g_runtime_path;
        }
        string _join_root_and_rest(const string& root, const string& rest)
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
        string _resolve_path(const char* path)
        {
            if (path[0] == '@')
            {
                string root = _get_runtime_path_copy();
                if (root.empty())
                    root = _get_host_path_copy();

                return _join_root_and_rest(root, path + 1);
            }

            if (path[0] == '!')
                return _join_root_and_rest(_get_host_path_copy(), path + 1);

            return path;
        }
    }
}

struct je_file_t
{
    je_raw_file_t m_native_file_handle;
    unique_ptr<fimg::File> m_image_file_handle;
    size_t m_file_length;

    je_file_t()
        : m_native_file_handle(nullptr)
        , m_file_length(0)
    {
    }
};

struct je_fimg_archive_t
{
    shared_ptr<const fimg::Archive> impl;
};

struct je_fimg_writer_t
{
    unique_ptr<fimg::Writer> impl;
};


void je_file_host_path_set(const char* path)
{
    assert(path != nullptr);
    lock_guard<mutex> lock(_g_path_mutex);
    _g_host_path = path;
}

void je_file_runtime_path_set(const char* path)
{
    assert(path != nullptr);
    lock_guard<mutex> lock(_g_path_mutex);
    _g_runtime_path = path;
}

const char* je_file_host_path_get()
{
    lock_guard<mutex> lock(_g_path_mutex);
    return _g_host_path.c_str();
}

const char* je_file_runtime_path_get()
{
    lock_guard<mutex> lock(_g_path_mutex);
    return _g_runtime_path.c_str();
}

/* OPTIONAL */ je_file_t* je_file_open(const char* path)
{
    assert(path != nullptr);
    if (path[0] == '\0')
        return nullptr;

    const string resolved_path = _resolve_path(path);

    size_t file_length = 0;
    const je_raw_file_t native_handle =
        native::open(resolved_path.c_str(), &file_length);

    if (native_handle != nullptr)
    {
        auto* file = new je_file_t;
        file->m_native_file_handle = native_handle;
        file->m_file_length = file_length;
        return file;
    }

    if (path[0] != '@' && path[0] != '!')
        return nullptr;

    shared_ptr<const fimg::Archive> archive;
    {
        shared_lock<shared_mutex> lock(_g_default_archive_mutex);
        archive = _g_default_archive;
    }

    if (archive == nullptr)
        return nullptr;

    const fimg::Entry* entry = archive->find(path);
    if (entry == nullptr)
        return nullptr;

    auto* file = new je_file_t;
    file->m_image_file_handle
        = make_unique<fimg::File>(move(archive), *entry);
    file->m_file_length = static_cast<size_t>(entry->size);
    return file;
}

void je_file_close(je_file_t* file)
{
    assert(file != nullptr);

    if (file->m_native_file_handle != nullptr)
    {
        auto close_state = native::close(file->m_native_file_handle);
        if (close_state != 0)
            debug::logerr("Fail to close file(%d:%d).", (int)close_state, (int)errno);
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
        return native::read(out_buffer, elem_size, count, file->m_native_file_handle);

    return file->m_image_file_handle->read(out_buffer, elem_size, count);
}

size_t je_file_tell(je_file_t* file)
{
    assert(file != nullptr);

    if (file->m_native_file_handle != nullptr)
        return native::tell(file->m_native_file_handle);

    return static_cast<size_t>(file->m_image_file_handle->tell());
}

bool je_file_seek(je_file_t* file, int64_t offset, je_file_seek_mode_t mode)
{
    assert(file != nullptr);

    if (file->m_native_file_handle != nullptr)
        return native::seek(file->m_native_file_handle, offset, mode) == 0;

    return file->m_image_file_handle->seek(offset, mode);
}

size_t je_file_size(je_file_t* file)
{
    assert(file != nullptr);
    return file->m_file_length;
}

bool je_fimg_default_archive_set(const char* root)
{
    shared_ptr<const fimg::Archive> archive;
    if (root != nullptr)
        archive = fimg::Archive::open(root);

    const bool loaded = root == nullptr || archive != nullptr;

    shared_ptr<const fimg::Archive> old_archive;
    {
        unique_lock<shared_mutex> lock(_g_default_archive_mutex);
        old_archive = move(_g_default_archive);
        _g_default_archive = move(archive);
    }

    return loaded;
}

je_fimg_archive_t* je_fimg_archive_open(const char* root)
{
    assert(root != nullptr);

    auto impl = fimg::Archive::open(root);
    if (impl == nullptr)
        return nullptr;

    auto* archive = new je_fimg_archive_t;
    archive->impl = move(impl);
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

    const fimg::Entry* entry = archive->impl->find(path);
    return entry == nullptr ? 0 : static_cast<size_t>(entry->size);
}

je_fimg_writer_t* je_fimg_writer_create(
    const char* root,
    size_t shard_size,
    const je_fimg_writer_options_t* options)
{
    assert(root != nullptr);

    fimg::WriterOptions resolved;
    if (options != nullptr)
    {
        resolved.compression_level = options->compression_level;
        resolved.min_compress_size = options->min_compress_size;
        resolved.skip_extensions = options->skip_extensions != nullptr
            ? options->skip_extensions
            : fimg::default_skip_extensions();
    }
    else
    {
        resolved.compression_level = 6;
        resolved.min_compress_size = 64;
        resolved.skip_extensions = fimg::default_skip_extensions();
    }

    auto impl = fimg::Writer::create(root, shard_size, move(resolved));

    auto* writer = new je_fimg_writer_t;
    writer->impl = move(impl);
    return writer;
}

const char* je_fimg_default_skip_extensions()
{
    return fimg::default_skip_extensions();
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
