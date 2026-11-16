#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs/cache.hpp"
#include "jeecs/file.hpp"
#include "jeecs/prefix.hpp"

#include "wo.h"

#include <list>
#include <optional>
#include <unordered_map>
#include <vector>

using namespace std;

// File reading & cache
WOORT_API woort_api wojeapi_read_file_all(void)
{
    const woort_U8CString filepath = woort_string(0);

    woort_vm* const last = woort_vm_swap(nullptr);
    {
        if (auto* file = je_file_open(filepath))
        {
            const size_t file_size = je_file_size(file);
            vector<char> readed_buf(file_size);
            auto readed_len = je_file_read(readed_buf.data(), sizeof(char), file_size, file);
            readed_buf.resize(readed_len);

            je_file_close(file);

            (void)woort_vm_swap(last);
            return woort_ret_option_buffer(readed_buf.data(), readed_buf.size());
        }
    }
    (void)woort_vm_swap(last);

    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_file_cache_write_all(void)
{
    bool ok = false;

    const woort_U8CString filepath = woort_string(0);
    size_t buflen;
    const void* buf = woort_buffer(1, &buflen);

    woort_vm* const last = woort_vm_swap(nullptr);
    if (auto* const cache = je_cache_file_create(filepath, 0, 1))
    {
        auto written_len = je_cache_file_write(buf, sizeof(char), buflen, cache);

        ok = buflen == written_len;
        je_cache_file_close(cache, ok);
    }
    (void)woort_vm_swap(last);

    return woort_ret_bool(ok);
}

WOORT_API woort_api wojeapi_file_cache_read_all(void)
{
    const woort_U8CString filepath = woort_string(0);

    woort_vm* const last = woort_vm_swap(nullptr);
    if (auto* cache = je_cache_file_load(filepath, 0, -1))
    {
        const size_t cache_size = je_file_size(cache);
        vector<char> readed_buf(cache_size);
        auto readed_len = je_file_read(readed_buf.data(), sizeof(char), cache_size, cache);
        readed_buf.resize(readed_len);

        je_file_close(cache);

        (void)woort_vm_swap(last);

        return woort_ret_option_buffer(readed_buf.data(), readed_buf.size());
    }
    (void)woort_vm_swap(last);

    return woort_ret_option_none();
}

// 二进制缓存文档写入器绑定：生成器脚本持有的第一个参数即
// je_writing_cache_file_t*，由 C 侧创建缓存文件后传入。
WOORT_API woort_api wojeapi_cache_binary_append_document(void)
{
    auto* file = static_cast<je_writing_cache_file_t*>(woort_pointer(0));
    return woort_ret_bool(je_cache_binary_append_document(file));
}

WOORT_API woort_api wojeapi_cache_binary_append_list(void)
{
    auto* file = static_cast<je_writing_cache_file_t*>(woort_pointer(0));
    return woort_ret_bool(je_cache_binary_append_list(file));
}

WOORT_API woort_api wojeapi_cache_binary_append_end(void)
{
    auto* file = static_cast<je_writing_cache_file_t*>(woort_pointer(0));
    return woort_ret_bool(je_cache_binary_append_end(file));
}

WOORT_API woort_api wojeapi_cache_binary_append_string(void)
{
    auto* file = static_cast<je_writing_cache_file_t*>(woort_pointer(0));
    return woort_ret_bool(je_cache_binary_append_string(file, woort_string(1)));
}

WOORT_API woort_api wojeapi_cache_binary_append_bool(void)
{
    auto* file = static_cast<je_writing_cache_file_t*>(woort_pointer(0));
    return woort_ret_bool(je_cache_binary_append_bool(file, woort_bool(1)));
}

WOORT_API woort_api wojeapi_cache_binary_append_int32(void)
{
    auto* file = static_cast<je_writing_cache_file_t*>(woort_pointer(0));
    return woort_ret_bool(
        je_cache_binary_append_int32(file, (int32_t)woort_int(1)));
}

WOORT_API woort_api wojeapi_cache_binary_append_float32(void)
{
    auto* file = static_cast<je_writing_cache_file_t*>(woort_pointer(0));
    return woort_ret_bool(
        je_cache_binary_append_float32(file, (float)woort_real(1)));
}

// int32 数组按 BUFFER 写入（瓦片网格/格 id 等大批量整数数据）。逐元素
// 写入 LIST 会让读取侧 list_index 反复从头扫描，因此统一走缓冲区。
WOORT_API woort_api wojeapi_cache_binary_append_int32_buffer(void)
{
    auto* file = static_cast<je_writing_cache_file_t*>(woort_pointer(0));

    const woort_value values = 1;
    const size_t count = woort_vec_len(values);

    vector<int32_t> buffer(count);
    for (size_t i = 0; i < count; ++i)
    {
        (void)woort_vec_get(WOORT_RETURN_SLOT, values, i);
        buffer[i] = (int32_t)woort_unbox_int(WOORT_RETURN_SLOT);
    }

    static const int32_t empty_buffer = 0;
    return woort_ret_bool(je_cache_binary_append_buffer(file,
        count > 0 ? (const void*)buffer.data() : (const void*)&empty_buffer,
        count * sizeof(int32_t)));
}

// 任意字节缓冲按 BUFFER 写入（图片像素等大批量数据）。
WOORT_API woort_api wojeapi_cache_binary_append_buffer(void)
{
    auto* file = static_cast<je_writing_cache_file_t*>(woort_pointer(0));

    size_t buflen = 0;
    const void* buf = woort_buffer(1, &buflen);

    return woort_ret_bool(je_cache_binary_append_buffer(file, buf, buflen));
}

// 无类型标记的裸字节区（文档之前的裸数据，如音频 PCM）。
WOORT_API woort_api wojeapi_cache_binary_append_raw_buffer(void)
{
    auto* file = static_cast<je_writing_cache_file_t*>(woort_pointer(0));

    size_t buflen = 0;
    const void* buf = woort_buffer(1, &buflen);

    return woort_ret_bool(je_cache_binary_append_raw_buffer(file, buf, buflen));
}

// FIMG resource-pack writer
WOORT_API woort_api wojeapi_fimg_writer_create(void)
{
    je_fimg_writer_options_t options;
    options.compression_level = (int)woort_int(2);
    options.min_compress_size = (size_t)woort_int(3);
    options.skip_extensions = woort_string(4);

    return woort_ret_pointer(je_fimg_writer_create(
        woort_string(0),
        (size_t)woort_int(1),
        &options));
}

WOORT_API woort_api wojeapi_fimg_default_skip_extensions(void)
{
    return woort_ret_string(je_fimg_default_skip_extensions());
}

WOORT_API woort_api wojeapi_fimg_writer_pack_file(void)
{
    auto* writer = static_cast<je_fimg_writer_t*>(woort_pointer(0));
    return woort_ret_bool(je_fimg_writer_pack_file(writer, woort_string(1), woort_string(2)));
}

WOORT_API woort_api wojeapi_fimg_writer_pack_buffer(void)
{
    auto* writer = static_cast<je_fimg_writer_t*>(woort_pointer(0));
    return woort_ret_bool(
        je_fimg_writer_pack_buffer(
            writer,
            woort_pointer(1),
            (size_t)woort_int(2),
            woort_string(3)));
}

WOORT_API woort_api wojeapi_fimg_writer_finish(void)
{
    auto* writer = static_cast<je_fimg_writer_t*>(woort_pointer(0));
    return woort_ret_bool(je_fimg_writer_finish(writer));
}

WOORT_API woort_api wojeapi_fimg_writer_abort(void)
{
    auto* writer = static_cast<je_fimg_writer_t*>(woort_pointer(0));
    je_fimg_writer_abort(writer);
    return woort_ret_void();
}
