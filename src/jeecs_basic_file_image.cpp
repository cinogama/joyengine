#define JE_IMPL
#include "jeecs/file.hpp"
#include "jeecs/logging.hpp"
#include "jeecs_basic_file_fimg_archive.hpp"
#include "jeecs_basic_file.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#endif

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
        string _g_user_path;

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
        bool _is_path_separator(char c)
        {
            return c == '/' || c == '\\';
        }
        bool _is_directory(const string& path)
        {
#if defined(_WIN32)
            struct _stat64 info;
            return _stat64(path.c_str(), &info) == 0
                && (info.st_mode & _S_IFDIR) != 0;
#else
            struct stat info;
            return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
        }
        bool _make_directory(const string& path)
        {
#if defined(_WIN32)
            const int result = _mkdir(path.c_str());
#else
            const int result = mkdir(path.c_str(), 0755);
#endif
            if (result == 0 || errno == EEXIST)
                return true;

            debug::logwarn("Failed to create directory '%s': %s",
                path.c_str(), strerror(errno));
            return false;
        }
        // Create `path` and every missing parent component, like mkdir -p.
        // std::filesystem is avoided because some ports (Emscripten, old
        // Android NDKs) support it poorly; every platform has a plain mkdir.
        bool _create_directories(const string& path)
        {
            size_t len = path.size();
            while (len > 0 && _is_path_separator(path[len - 1]))
                --len;
            if (len == 0)
                return false;

            string current;
            current.reserve(len);

            size_t pos = 0;
#if defined(_WIN32)
            if (len >= 2 && path[1] == ':')
            {
                // "C:" is a drive prefix, not a directory to create; keep it
                // as the root and append components after it.
                current.assign(path, 0, 2);
                pos = 2;
            }
            else if (len >= 2 && _is_path_separator(path[0]) && _is_path_separator(path[1]))
            {
                // UNC path "\\server\share\...": the server and share already
                // exist, only what follows them may need to be created.
                size_t separators = 0;
                while (pos < len)
                {
                    if (_is_path_separator(path[pos]) && ++separators == 4)
                        break;
                    ++pos;
                }
                if (separators < 3)
                    return false;
                if (separators == 3)
                    return _is_directory(string(path, 0, len));

                current.assign(path, 0, pos + 1);
                ++pos;
            }
            else if (_is_path_separator(path[0]))
            {
                // Root-relative path "\foo\bar": the root of the current drive.
                current.assign(1, '\\');
                pos = 1;
            }
#else
            if (path[0] == '/')
            {
                current.assign(1, '/');
                pos = 1;
            }
#endif

            while (pos < len)
            {
                while (pos < len && _is_path_separator(path[pos]))
                    ++pos;
                if (pos >= len)
                    break;

                const size_t begin = pos;
                while (pos < len && !_is_path_separator(path[pos]))
                    ++pos;

                if (!current.empty() && !_is_path_separator(current.back()))
                    current.push_back('/');
                current.append(path, begin, pos - begin);

                if (!_make_directory(current))
                    return false;
            }

            // mkdir treats EEXIST as success, which also accepts an existing
            // regular file; verify the final component is a directory.
            return _is_directory(current);
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

void je_file_user_path_set(const char* path)
{
    assert(path != nullptr);
    lock_guard<mutex> lock(_g_path_mutex);
    _g_user_path = path;
}

bool je_file_user_path_assure()
{
    string path;
    {
        lock_guard<mutex> lock(_g_path_mutex);
        path = _g_user_path;
    }

    if (path.empty())
    {
        debug::logwarn("User path is empty, failed to create user path directory.");
        return false;
    }

    return _create_directories(path);
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

const char* je_file_user_path_get()
{
    lock_guard<mutex> lock(_g_path_mutex);
    return _g_user_path.c_str();
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
        = make_unique<fimg::File>(std::move(archive), *entry);
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
        old_archive = std::move(_g_default_archive);
        _g_default_archive = std::move(archive);
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

    auto impl = fimg::Writer::create(root, shard_size, std::move(resolved));

    auto* writer = new je_fimg_writer_t;
    writer->impl = std::move(impl);
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
