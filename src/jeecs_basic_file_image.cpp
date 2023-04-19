#define JE_IMPL
#include "jeecs.hpp"

#include <cstdio>
#include <mutex>
#include <sys/stat.h>

///
#include<cstdio>
#include<cstdlib>
#include<cstring>

#include<memory>
#include<map>
#include<string>
#include<iostream>
#include<filesystem>

using length_t = uint64_t;

constexpr length_t DEFAULT_IMAGE_SIZE = 1 * 1024 * 1024;
constexpr const char* FIMAGE_TABLE_EXTENSION_NAME = ".jeimgidx4";
constexpr const char* FIMAGE_FILE_EXTENSION_NAME = ".jeimg4";

struct fimg_image_head
{
    unsigned char magic_head[12] = { 'C','I','N','O','G','A','M','A','F','I','M','G' };      // should be "CINOGAMAFIMG", 12
    unsigned char version[4] = { 1,0,0,0 };               // 1 0 0 0
    length_t file_count = 0;
    length_t disk_count = 0;
    length_t MAX_FILE_SINGLE_IMG_SIZE = DEFAULT_IMAGE_SIZE; // byte

    bool valid() const
    {
        return 0 == memcmp(magic_head, "CINOGAMAFIMG", sizeof(magic_head));
    }
};

struct fimg_img_index
{
    length_t img_index;
    length_t img_offset;
    length_t file_size;
};

struct fimg_data
{
    fimg_image_head fimg_head;


    std::string path;
    std::map<std::string, fimg_img_index> file_map;

};
using fimg_img = fimg_data;

struct fimg_creating_context
{
    fimg_img* image;
    std::string writing_path;
    unsigned char* writing_buffer;
    size_t writing_offset;
};

struct jeecs_fimg_file
{
    std::vector<FILE*> fds;

    size_t f_index = 0;
    size_t f_diff_ptr = 0;
    size_t f_size = 0;      // just like const
    size_t i_max_file_sz = 0;
    size_t nf_index = 0;
    size_t nf_diff_ptr = 0;
    size_t nreaded_sz = 0;

};

fimg_img* fimg_open_img(const char* path)
{
    using namespace std;

    // Read img from specified path or current path.
    // If you don't want to specify the path, you can call 'fimg_read_img' with null;
    FILE* fimg_fp = fopen((path + "/fimg_table"s + FIMAGE_TABLE_EXTENSION_NAME).c_str(), "rb");

    if (!fimg_fp)
        return nullptr;

    fimg_img* fimgdata = new fimg_img;
    fimgdata->path = path;

    fread(&fimgdata->fimg_head, sizeof(fimgdata->fimg_head), 1, fimg_fp);

    auto filecount = fimgdata->fimg_head.file_count;
    if (fimgdata->fimg_head.valid())
    {
        while (filecount--)
        {
            std::string rel_path;

            char chcode;
            while (fread(&chcode, sizeof(char), 1, fimg_fp))
            {
                if (chcode)
                    rel_path += chcode;
                else
                    break;
            }
            // read file rel_path end.

            fimg_img_index findex;
            fread(&findex, sizeof(fimg_img_index), 1, fimg_fp);

            fimgdata->file_map[rel_path] = findex;
        }
    }
    else
        goto load_fail;

    fclose(fimg_fp);
    return fimgdata;

load_fail:
    fclose(fimg_fp);
    return nullptr;
}

void fimg_close_img(fimg_img* img)
{
    delete img;
}
size_t fimg_save_buffer_to_img_impl(fimg_creating_context* ctx, const void* buffer, size_t buffer_len)
{
    size_t real_read_sz = 0;
    size_t remain_buf_len = buffer_len;
    do
    {
        size_t buffer_free_size = ctx->image->fimg_head.MAX_FILE_SINGLE_IMG_SIZE - ctx->writing_offset;
        real_read_sz = std::min(buffer_free_size, remain_buf_len);

        memcpy(ctx->writing_buffer + ctx->writing_offset, buffer, real_read_sz);

        remain_buf_len -= real_read_sz;
        ctx->writing_offset += real_read_sz;

        if (ctx->writing_offset >= ctx->image->fimg_head.MAX_FILE_SINGLE_IMG_SIZE)
        {
            FILE* imgwrite = fopen((ctx->writing_path + "/disk-" + std::to_string(ctx->image->fimg_head.disk_count) + FIMAGE_FILE_EXTENSION_NAME).c_str(), "wb");

            fwrite(ctx->writing_buffer, 1, ctx->image->fimg_head.MAX_FILE_SINGLE_IMG_SIZE, imgwrite);
            fclose(imgwrite);

            ctx->writing_offset = 0;
            ctx->image->fimg_head.disk_count++;
        }

    } while (real_read_sz != 0);

    return buffer_len;
}
size_t fimg_save_file_to_img_impl(fimg_creating_context* ctx, const char* file_path)
{
    FILE* fp = fopen(file_path, "rb");
    if (fp)
    {
        size_t total_read_sz = 0;
        size_t real_read_sz;
        do
        {
            size_t buffer_free_size = ctx->image->fimg_head.MAX_FILE_SINGLE_IMG_SIZE - ctx->writing_offset;
            real_read_sz = fread(ctx->writing_buffer + ctx->writing_offset, 1, buffer_free_size, fp);

            total_read_sz += real_read_sz;
            ctx->writing_offset += real_read_sz;
            if (ctx->writing_offset >= ctx->image->fimg_head.MAX_FILE_SINGLE_IMG_SIZE)
            {
                FILE* imgwrite = fopen((ctx->writing_path + "/disk-" + std::to_string(ctx->image->fimg_head.disk_count) + FIMAGE_FILE_EXTENSION_NAME).c_str(), "wb");

                fwrite(ctx->writing_buffer, 1, ctx->image->fimg_head.MAX_FILE_SINGLE_IMG_SIZE, imgwrite);
                fclose(imgwrite);

                ctx->writing_offset = 0;
                ctx->image->fimg_head.disk_count++;
            }

        } while (real_read_sz != 0);

        fclose(fp);
        return total_read_sz;
    }
    jeecs::debug::logerr("Failed to read '%s' when trying to pack files.", file_path);
    return (size_t)-1;
}

fimg_creating_context* fimg_create_new_img_for_storing(const char* path, const char* storing_path, size_t packsize)
{
    using namespace std;

    fimg_creating_context* ctx = new fimg_creating_context();
    ctx->image = new fimg_img;
    ctx->writing_path = storing_path;
    ctx->writing_offset = 0;
    ctx->image->path = path;
    ctx->image->fimg_head.MAX_FILE_SINGLE_IMG_SIZE =
        packsize == 0 ? DEFAULT_IMAGE_SIZE : packsize;
    ctx->writing_buffer = new unsigned char[ctx->image->fimg_head.MAX_FILE_SINGLE_IMG_SIZE];

    assert(ctx->image->fimg_head.disk_count == 0);
    assert(ctx->image->fimg_head.file_count == 0);

    return ctx;
}

bool fimg_storing_buffer_to_img(fimg_creating_context* ctx, const void* buffer, size_t len, const char* aimpath)
{
    using namespace std;

    size_t this_file_img_index = ctx->image->fimg_head.disk_count;
    size_t this_file_diff_count = ctx->writing_offset;

    size_t filesz = fimg_save_buffer_to_img_impl(ctx, buffer, len);

    if (filesz != (size_t)-1)
    {
        ++ctx->image->fimg_head.file_count;

        ctx->image->file_map[aimpath] =
        {
            this_file_img_index,
            this_file_diff_count,
            filesz
        };
        return true;
    }
    return false;
}
bool fimg_storing_file_to_img(fimg_creating_context* ctx, const char* filepath, const char* aimpath)
{
    using namespace std;

    size_t this_file_img_index = ctx->image->fimg_head.disk_count;
    size_t this_file_diff_count = ctx->writing_offset;

    size_t filesz = fimg_save_file_to_img_impl(ctx, filepath);

    if (filesz != (size_t)-1)
    {
        ++ctx->image->fimg_head.file_count;

        ctx->image->file_map[aimpath] =
        {
            this_file_img_index,
            this_file_diff_count,
            filesz
        };
        return true;
    }
    return false;
}

void fimg_finish_saving_img_and_close(fimg_creating_context* ctx)
{
    using namespace std;

    // Save last FIMAGE_FILE_EXTENSION_NAME
    FILE* lastimgwrite = fopen((ctx->writing_path + "/disk-" + std::to_string(ctx->image->fimg_head.disk_count) + FIMAGE_FILE_EXTENSION_NAME).c_str(), "wb");
    fwrite(ctx->writing_buffer, 1, ctx->writing_offset, lastimgwrite);
    fclose(lastimgwrite);

    // Save FIMAGE_TABLE_EXTENSION_NAME
    FILE* imgwrite = fopen((ctx->writing_path + "/fimg_table"s + FIMAGE_TABLE_EXTENSION_NAME).c_str(), "wb");
    ++ctx->image->fimg_head.disk_count;
    fwrite(&ctx->image->fimg_head, sizeof(ctx->image->fimg_head), 1, imgwrite);
    for (auto& fdata : ctx->image->file_map)
    {
        fwrite(fdata.first.c_str(), sizeof(unsigned char), fdata.first.size() + 1, imgwrite);
        fwrite(&fdata.second, sizeof(fdata.second), 1, imgwrite);
    }
    fclose(imgwrite);

    fimg_close_img(ctx->image);

    delete[] ctx->writing_buffer;
    delete ctx;
}

void fimg_reset(jeecs_fimg_file* file)
{
    file->nf_diff_ptr = file->f_diff_ptr;
    file->nf_index = 0;
    file->nreaded_sz = 0;

    for (auto fptr : file->fds)
        fseek(fptr, 0, SEEK_SET);

    if (file->fds.size())
        fseek(file->fds[0], (long)file->f_diff_ptr, SEEK_SET);
}

size_t fimg_read(void* buffer, size_t elemsize, size_t count, jeecs_fimg_file* file)
{
    size_t readed = 0;
    unsigned char* write_byte_buffer = (unsigned char*)buffer;

    size_t remain_read_size = std::min(elemsize * count, file->f_size - file->nreaded_sz);

    while (remain_read_size)
    {
        size_t this_time_read_sz = std::min(remain_read_size, file->i_max_file_sz - file->nf_diff_ptr);
        fread(write_byte_buffer, 1, this_time_read_sz, file->fds[file->nf_index]);

        remain_read_size -= this_time_read_sz;
        write_byte_buffer += this_time_read_sz;
        file->nf_diff_ptr += this_time_read_sz;
        file->nreaded_sz += this_time_read_sz;

        readed += this_time_read_sz;

        if (file->nf_diff_ptr >= file->i_max_file_sz)
        {
            file->nf_index += 1;
            file->nf_diff_ptr = 0;
        }
    }

    assert(readed % elemsize == 0);
    return readed / elemsize;
}

void fimg_close_file(jeecs_fimg_file* file)
{
    for (auto fp : file->fds)
        if (fp)
            fclose(fp);
    delete file;
}

// used for read file from img
jeecs_fimg_file* fimg_open_file(fimg_img* img, const char* fpath)
{
    auto itor = img->file_map.find(fpath);
    if (itor != img->file_map.end())
    {
        jeecs_fimg_file* fptr = new jeecs_fimg_file{ {} };

        auto& f = itor->second;
        size_t img_index = (size_t)f.img_index;
        size_t start_diff = (size_t)f.img_offset;
        for (size_t img_byte = 0; img_byte < f.file_size;)
        {
            auto disk_path = img->path + "/disk-" + std::to_string(img_index) + FIMAGE_FILE_EXTENSION_NAME;
            auto* disk_file_handle = fopen(disk_path.c_str(), "rb");
            if (disk_file_handle == nullptr)
            {
                jeecs::debug::logerr("Unable to open disk-%zu: '%s' when trying to read '%s'.", img_index, disk_path.c_str(), fpath);
                fimg_close_file(fptr);
                return nullptr;
            }

            fptr->fds.push_back(disk_file_handle);
            img_index++;

            img_byte += (size_t)(img->fimg_head.MAX_FILE_SINGLE_IMG_SIZE - start_diff);

            start_diff = 0;
        }
        fptr->f_size = (size_t)f.file_size;
        fptr->nf_index = fptr->f_index = 0;
        fptr->nf_diff_ptr = fptr->f_diff_ptr = (size_t)f.img_offset;
        fptr->i_max_file_sz = (size_t)img->fimg_head.MAX_FILE_SINGLE_IMG_SIZE;
        fimg_reset(fptr);
        return fptr;
    }
    return nullptr;
}

///

std::string _je_runtime_path = "!";
fimg_img* _je_runtime_file_image = nullptr;
std::shared_mutex _je_runtime_path_and_image_mx;

void jeecs_file_set_runtime_path(const char* path)
{
    std::lock_guard g1(_je_runtime_path_and_image_mx);

    _je_runtime_path = path;
    if (_je_runtime_file_image != nullptr)
        fimg_close_img(_je_runtime_file_image);

    _je_runtime_file_image = fimg_open_img(path);
}

const char* jeecs_file_get_runtime_path()
{
    std::shared_lock g1(_je_runtime_path_and_image_mx);

    return _je_runtime_path.c_str();
}

jeecs_file* jeecs_file_open(const char* path)
{
    // TODO: Open file in work path.
    std::string path_str = path;

    assert(path_str.empty() == false);

    if (path_str[0] == '@')
    {
        std::shared_lock sg1(_je_runtime_path_and_image_mx);
        // 1. Local file, trying to find file from file-image.
        if (_je_runtime_file_image != nullptr)
        {
            auto* img_file = fimg_open_file(_je_runtime_file_image, path);
            if (img_file != nullptr)
            {
                jeecs_file* jefhandle = new jeecs_file();
                jefhandle->m_native_file_handle = nullptr;
                jefhandle->m_image_file_handle = img_file;
                jefhandle->m_file_length = img_file->f_size;

                return jefhandle;
            }
        }

        // 2. Not found, try find from local runtime path.
        path_str = _je_runtime_path + path_str.substr(1);
    }
    if (path_str[0] == '!')
        path_str = wo_exe_path() + path_str.substr(1);

    FILE* fhandle = fopen(path_str.c_str(), "rb");
    if (fhandle)
    {
        jeecs_file* jefhandle = new jeecs_file();
        jefhandle->m_native_file_handle = fhandle;
        jefhandle->m_image_file_handle = nullptr;
        struct stat cfstat;
        stat(path_str.c_str(), &cfstat);
        jefhandle->m_file_length = cfstat.st_size;

        return jefhandle;
    }
    jeecs::debug::logerr("Fail to open file: '%s'(%d).", path, (int)errno);
    return nullptr;
}
void jeecs_file_close(jeecs_file* file)
{
    assert(file != nullptr);

    if (file->m_native_file_handle != nullptr)
    {
        auto close_state = fclose(file->m_native_file_handle);
        if (close_state != 0)
            jeecs::debug::logerr("Fail to close file(%d:%d).", (int)close_state, (int)errno);
    }
    else
    {
        assert(file->m_image_file_handle != nullptr);
        return fimg_close_file(file->m_image_file_handle);
    }
    delete file;
}
size_t jeecs_file_read(
    void* out_buffer,
    size_t elem_size,
    size_t count,
    jeecs_file* file)
{
    if (file->m_native_file_handle != nullptr)
        return fread(out_buffer, elem_size, count, file->m_native_file_handle);
    else
    {
        assert(file->m_image_file_handle != nullptr);
        return fimg_read(out_buffer, elem_size, count, file->m_image_file_handle);
    }
}

fimg_creating_context* jeecs_file_image_begin(const char* path, const char* storing_path, size_t max_image_size)
{
    return fimg_create_new_img_for_storing(path, storing_path, max_image_size);
}
bool jeecs_file_image_pack_file(fimg_creating_context* context, const char* filepath, const char* packingpath)
{
    return fimg_storing_file_to_img(context, filepath, packingpath);
}
bool jeecs_file_image_pack_buffer(fimg_creating_context* context, const void* buffer, size_t len, const char* packingpath)
{
    return fimg_storing_buffer_to_img(context, buffer, len, packingpath);
}
void jeecs_file_image_finish(fimg_creating_context* context)
{
    fimg_finish_saving_img_and_close(context);
}