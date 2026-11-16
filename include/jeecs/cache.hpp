#pragma once

// ============================================================================
// jeecs/cache.hpp —— 缓存：je_cache_file_* 缓存文件与 je_cache_binary_* 二进制缓存文档 C API
// =============================================================================

#include "jeecs/prefix.hpp"
#include "jeecs/file.hpp"

JE_FORCE_CAPI

// If ignore_crc64 == true, cache will always work even if origin file changed.

/*
je_cache_file_load [基本接口]
尝试读取 filepath 对应的缓存文件，将校验缓存文件的格式版本和校验码
缓存文件存在且校验通过则成功读取，否则返回nullptr
    * 若 virtual_crc64 被指定为 -1，则忽略检验原始文件一致性
    * 若 virtual_crc64 被指定为 0，则使用 filepath 指定文件的内容计算校验值
    * 其他情况，使用 virtual_crc64 直接指定校验值
    * 打开后的缓存文件需要使用 je_file_close 关闭
    * 使用 je_file_read 读取数据
请参见：
    je_file_read
    je_file_close
*/
JE_API /* OPTIONAL */ je_file_t* je_cache_file_load(
    const char* filepath,
    uint32_t format_version,
    uint64_t virtual_crc64);

// If usecrc64 != 0, cache file will use it instead of reading from origin file.

typedef struct je_writing_cache_file_t je_writing_cache_file_t;

/*
je_cache_file_create [基本接口]
为 filepath 指定的文件创建缓存文件，将覆盖已有的缓存文件（如果已有的话）
若创建文件失败，则返回nullptr
    * 若 usecrc64 被指定为 0，则自动计算 filepath 指定文件的校验值
    * 否则直接使用 usecrc64 作为校验值
    * 创建时缓存主版本号留空占位，此时的缓存文件始终无效，
      直到 je_cache_file_close 以 finished == true 关闭回写主版本号后
      缓存才生效
    * 打开后的缓存文件需要使用 je_cache_file_close 关闭
    * 使用 je_cache_file_write 写入数据
请参见：
    je_cache_file_close
    je_cache_file_write
*/
JE_API /* OPTIONAL */ je_writing_cache_file_t* je_cache_file_create(
    const char* filepath,
    uint32_t format_version,
    uint64_t usecrc64);

/*
je_cache_file_write [基本接口]
向已创建的缓存文件中写入若干个指定大小的元素，返回成功写入的元素数量
*/
JE_API size_t je_cache_file_write(
    const void* write_buffer,
    size_t elem_size,
    size_t count,
    je_writing_cache_file_t* file);

/*
je_cache_file_close [基本接口]
关闭创建出的缓存文件
    * finished 为 true 时表示缓存内容已完整写入，关闭前会回写缓存主版本号
      使缓存生效（je_cache_file_create 创建时主版本号留空占位）
    * finished 为 false 时表示缓存写入失败，直接关闭且不回写主版本号，
      这样关闭的缓存文件无法通过 je_cache_file_load 的校验，始终无效
请参见：
    je_cache_file_create
*/
JE_API void je_cache_file_close(je_writing_cache_file_t* file, bool finished);
/////////////////////////// CACHE BINARY DOCUMENT /////////////////////////////////

/*
je_cache_binary_value_type_t [类型]
二进制缓存文档中的值类型标记。每个值都从一个 1 字节的类型标记开始：
    * BOOL ~ FLOAT64 —— 标量，紧跟固定宽度的数据，按本机字节序存储；
                         BOOL 固定占用 1 字节，读取时非 0 即 true
    * STRING / BUFFER —— 紧跟 uint64 长度和对应长度的内容
    * LIST / DOCUMENT —— 容器，紧跟若干子值，以 JE_CACHE_BINARY_VALUE_END 结束
标记值即缓存文件格式的一部分，只能追加新类型，不可重排或修改已有值。
缓存文件由本机生成，不保证跨端序兼容。
请参见：
    je_cache_binary_append_document
    je_cache_binary_document_index
*/
typedef enum je_cache_binary_value_type_t
{
    JE_CACHE_BINARY_VALUE_BOOL = 0,
    JE_CACHE_BINARY_VALUE_INT8 = 1,
    JE_CACHE_BINARY_VALUE_INT16 = 2,
    JE_CACHE_BINARY_VALUE_INT32 = 3,
    JE_CACHE_BINARY_VALUE_INT64 = 4,
    JE_CACHE_BINARY_VALUE_UINT8 = 5,
    JE_CACHE_BINARY_VALUE_UINT16 = 6,
    JE_CACHE_BINARY_VALUE_UINT32 = 7,
    JE_CACHE_BINARY_VALUE_UINT64 = 8,
    JE_CACHE_BINARY_VALUE_FLOAT32 = 9,
    JE_CACHE_BINARY_VALUE_FLOAT64 = 10,
    JE_CACHE_BINARY_VALUE_STRING = 11,
    JE_CACHE_BINARY_VALUE_BUFFER = 12,
    JE_CACHE_BINARY_VALUE_LIST = 13,
    JE_CACHE_BINARY_VALUE_DOCUMENT = 14,
    JE_CACHE_BINARY_VALUE_END = 15,
} je_cache_binary_value_type_t;

/*
je_cache_binary_document_t [类型]
二进制缓存文档的只读句柄，位置指向文档的第一个键（DOCUMENT 标记之后）
    * 文档由若干 [STRING 键][值] 组成，以 END 结束
    * 句柄不拥有文件，句柄有效期间对应的 je_file_t 必须保持打开
    * 读取接口会移动文件读取位置，不要与顺序读取混用
字段由引擎内部维护，请勿直接修改
请参见：
    je_cache_binary_document_open
    je_cache_binary_document_index
*/
typedef struct je_cache_binary_document_t
{
    je_file_t* m_file;
    size_t m_location;
} je_cache_binary_document_t;

/*
je_cache_binary_list_t [类型]
二进制缓存列表的只读句柄，位置指向列表的第一个元素（LIST 标记之后）
    * 列表由若干值组成，以 END 结束
    * 句柄不拥有文件，句柄有效期间对应的 je_file_t 必须保持打开
    * 读取接口会移动文件读取位置，不要与顺序读取混用
字段由引擎内部维护，请勿直接修改
请参见：
    je_cache_binary_list_open
    je_cache_binary_list_index
*/
typedef struct je_cache_binary_list_t
{
    je_file_t* m_file;
    size_t m_location;
} je_cache_binary_list_t;

/*
je_cache_binary_list_iterator_t [类型]
二进制缓存列表的顺序遍历游标，位置指向下一个尚未读取的元素
    * 由 je_cache_binary_list_iterator_open 从列表句柄初始化
    * 不拥有文件，游标有效期间对应的 je_file_t 必须保持打开
    * 读取接口会移动文件读取位置，不要与顺序读取混用
字段由引擎内部维护，请勿直接修改
请参见：
    je_cache_binary_list_iterator_open
    je_cache_binary_list_iterator_is_end
    je_cache_binary_list_iterator_read
    je_cache_binary_list_iterator_advance
*/
typedef struct je_cache_binary_list_iterator_t
{
    je_file_t* m_file;
    size_t m_location;
} je_cache_binary_list_iterator_t;

/*
je_cache_binary_string_t [类型]
二进制缓存字符串值的只读句柄，位置指向该值自己的 STRING 标记
    * 句柄不拥有文件，句柄有效期间对应的 je_file_t 必须保持打开
    * 读取接口会移动文件读取位置，不要与顺序读取混用
字段由引擎内部维护，请勿直接修改
请参见：
    je_cache_binary_string_read
*/
typedef struct je_cache_binary_string_t
{
    je_file_t* m_file;
    size_t m_location;
} je_cache_binary_string_t;

/*
je_cache_binary_buffer_t [类型]
二进制缓存缓冲区值的只读句柄，位置指向该值自己的 BUFFER 标记
    * 句柄不拥有文件，句柄有效期间对应的 je_file_t 必须保持打开
    * 读取接口会移动文件读取位置，不要与顺序读取混用
字段由引擎内部维护，请勿直接修改
请参见：
    je_cache_binary_buffer_read
*/
typedef struct je_cache_binary_buffer_t
{
    je_file_t* m_file;
    size_t m_location;
} je_cache_binary_buffer_t;

/*
je_cache_binary_document_open [基本接口]
从文件当前位置读取一个 DOCUMENT 标记并初始化文档句柄
    * 当前位置不是 DOCUMENT 标记时返回 false
    * 成功时 out_doc 被填入文档句柄，文件位置移动到文档的第一个键
请参见：
    je_cache_binary_document_index
    je_cache_binary_list_open
*/
JE_API bool je_cache_binary_document_open(
    je_file_t* file,
    je_cache_binary_document_t* out_doc);

/*
je_cache_binary_list_open [基本接口]
从文件当前位置读取一个 LIST 标记并初始化列表句柄
    * 当前位置不是 LIST 标记时返回 false
    * 成功时 out_list 被填入列表句柄，文件位置移动到列表的第一个元素
请参见：
    je_cache_binary_list_index
    je_cache_binary_document_open
*/
JE_API bool je_cache_binary_list_open(
    je_file_t* file,
    je_cache_binary_list_t* out_list);

/*
je_cache_binary_string_read [基本接口]
从字符串句柄读取字符串内容
    * inout_dst_len 传入 dst 的容量，成功返回时被改写为字符串的实际长度
    * 容量不足时只复制前 inout_dst_len 个字节；复制后仍有空位时才写入 '\0'
    * dst == nullptr 时 inout_dst_len 必须为 0，此时仅查询字符串长度
    * 实际长度可能大于容量，调用方可据此重新分配后再次读取
    * 读取失败返回 false
请参见：
    je_cache_binary_document_index
    je_cache_binary_list_index
*/
JE_API bool je_cache_binary_string_read(
    /* OPTIONAL: If *inout_dst_len == 0 */ char* dst,
    size_t* inout_dst_len,
    const je_cache_binary_string_t* str);

/*
je_cache_binary_buffer_read [基本接口]
从缓冲区句柄读取缓冲区内容
    * inout_dst_len 传入 dst 的容量，成功返回时被改写为缓冲区的实际长度
    * 容量不足时只复制前 inout_dst_len 个字节，缓冲区没有结尾符
    * dst == nullptr 时 inout_dst_len 必须为 0，此时仅查询缓冲区长度
    * 实际长度可能大于容量，调用方可据此重新分配后再次读取
    * 读取失败返回 false
请参见：
    je_cache_binary_buffer_t
    je_cache_binary_document_index
*/
JE_API bool je_cache_binary_buffer_read(
    /* OPTIONAL: If *inout_dst_len == 0 */ void* dst,
    size_t* inout_dst_len,
    const je_cache_binary_buffer_t* buf);

/*
je_cache_binary_document_index [基本接口]
查找文档中指定键对应的值
    * expected_type 指定期望的值类型，实际类型不符时返回 false
    * out_value 按 expected_type 解释：
        BOOL 写入 bool
        INT8 / INT16 / INT32 / INT64 写入 int8_t / int16_t / int32_t / int64_t
        UINT8 / UINT16 / UINT32 / UINT64 写入 uint8_t / uint16_t / uint32_t / uint64_t
        FLOAT32 / FLOAT64 写入 float / double
        STRING 写入 je_cache_binary_string_t
        BUFFER 写入 je_cache_binary_buffer_t
        LIST 写入 je_cache_binary_list_t
        DOCUMENT 写入 je_cache_binary_document_t
    * 同一个键出现多次时使用第一个
    * 键不存在、类型不符或读取失败时返回 false
请参见：
    je_cache_binary_value_type_t
    je_cache_binary_string_read
    je_cache_binary_buffer_read
    je_cache_binary_list_index
*/
JE_API bool je_cache_binary_document_index(
    const je_cache_binary_document_t* document,
    const char* key,
    je_cache_binary_value_type_t expected_type,
    void* out_value);

/*
je_cache_binary_list_index [基本接口]
获取列表中指定下标的值
    * expected_type 和 out_value 的约定与 je_cache_binary_document_index 相同
    * 下标越界、类型不符或读取失败时返回 false
请参见：
    je_cache_binary_document_index
*/
JE_API bool je_cache_binary_list_index(
    const je_cache_binary_list_t* list,
    size_t index,
    je_cache_binary_value_type_t expected_type,
    void* out_value);

/*
je_cache_binary_list_iterator_open [基本接口]
从列表句柄初始化顺序遍历游标
    * 游标指向列表的第一个元素（可能已经是 END）
    * 读取接口会移动文件读取位置，不要与顺序读取混用
请参见：
    je_cache_binary_list_iterator_t
*/
JE_API bool je_cache_binary_list_iterator_open(
    const je_cache_binary_list_t* list,
    je_cache_binary_list_iterator_t* out_iterator);

/*
je_cache_binary_list_iterator_is_end [基本接口]
判断游标是否已经到达列表末尾
    * 到达 END 返回 true
    * 读取失败（越界/截断）返回 false，随后 iterator_read 会失败，调用方
      据此拒绝损坏的缓存
    * 读取不会移动游标的逻辑位置
请参见：
    je_cache_binary_list_iterator_t
*/
JE_API bool je_cache_binary_list_iterator_is_end(
    const je_cache_binary_list_iterator_t* iterator);

/*
je_cache_binary_list_iterator_read [基本接口]
读取游标当前元素
    * expected_type 和 out_value 的约定与 je_cache_binary_document_index 相同
    * 不移动游标；类型不符或读取失败时返回 false
请参见：
    je_cache_binary_list_iterator_advance
*/
JE_API bool je_cache_binary_list_iterator_read(
    const je_cache_binary_list_iterator_t* iterator,
    je_cache_binary_value_type_t expected_type,
    void* out_value);

/*
je_cache_binary_list_iterator_advance [基本接口]
把游标移动到下一个元素
    * 已到达 END 或读取失败时返回 false
请参见：
    je_cache_binary_list_iterator_read
*/
JE_API bool je_cache_binary_list_iterator_advance(
    je_cache_binary_list_iterator_t* inout_iterator);

/*
je_cache_binary_append_document [基本接口]
向缓存文件追加一个 DOCUMENT 标记
    * 之后依次写入 [STRING 键][值]，最后由 je_cache_binary_append_end 结束
    * 写入失败返回 false
请参见：
    je_cache_binary_append_end
    je_cache_binary_append_string
*/
JE_API bool je_cache_binary_append_document(je_writing_cache_file_t* file);

/*
je_cache_binary_append_list [基本接口]
向缓存文件追加一个 LIST 标记
    * 之后依次写入各元素，最后由 je_cache_binary_append_end 结束
    * 写入失败返回 false
请参见：
    je_cache_binary_append_end
*/
JE_API bool je_cache_binary_append_list(je_writing_cache_file_t* file);

/*
je_cache_binary_append_end [基本接口]
向缓存文件追加一个 END 标记，结束最近一个尚未结束的列表或文档
    * 写入失败返回 false
请参见：
    je_cache_binary_append_document
    je_cache_binary_append_list
*/
JE_API bool je_cache_binary_append_end(je_writing_cache_file_t* file);

/*
je_cache_binary_append_string [基本接口]
向缓存文件追加一个字符串值
    * 写入 uint64 长度和字符串内容，不写入结尾的 '\0'，长度由 strlen 决定
    * 写入失败返回 false
请参见：
    je_cache_binary_string_read
*/
JE_API bool je_cache_binary_append_string(
    je_writing_cache_file_t* file,
    const char* str);

/*
je_cache_binary_append_buffer [基本接口]
向缓存文件追加一个缓冲区值
    * 写入 uint64 长度和 buflen 个字节
    * 写入失败返回 false
请参见：
    je_cache_binary_buffer_read
*/
JE_API bool je_cache_binary_append_buffer(
    je_writing_cache_file_t* file,
    const void* buf,
    size_t buflen);

/*
je_cache_binary_append_bool [基本接口]
向缓存文件追加一个 bool 值
    * 固定写入 1 字节，true 写为 1，false 写为 0
    * 写入失败返回 false
*/
JE_API bool je_cache_binary_append_bool(
    je_writing_cache_file_t* file,
    bool val);

/*
je_cache_binary_append_int8 [基本接口]
向缓存文件追加一个 int8 值，写入失败返回 false
*/
JE_API bool je_cache_binary_append_int8(
    je_writing_cache_file_t* file,
    int8_t val);

/*
je_cache_binary_append_int16 [基本接口]
向缓存文件追加一个 int16 值，写入失败返回 false
*/
JE_API bool je_cache_binary_append_int16(
    je_writing_cache_file_t* file,
    int16_t val);

/*
je_cache_binary_append_int32 [基本接口]
向缓存文件追加一个 int32 值，写入失败返回 false
*/
JE_API bool je_cache_binary_append_int32(
    je_writing_cache_file_t* file,
    int32_t val);

/*
je_cache_binary_append_int64 [基本接口]
向缓存文件追加一个 int64 值，写入失败返回 false
*/
JE_API bool je_cache_binary_append_int64(
    je_writing_cache_file_t* file,
    int64_t val);

/*
je_cache_binary_append_uint8 [基本接口]
向缓存文件追加一个 uint8 值，写入失败返回 false
*/
JE_API bool je_cache_binary_append_uint8(
    je_writing_cache_file_t* file,
    uint8_t val);

/*
je_cache_binary_append_uint16 [基本接口]
向缓存文件追加一个 uint16 值，写入失败返回 false
*/
JE_API bool je_cache_binary_append_uint16(
    je_writing_cache_file_t* file,
    uint16_t val);

/*
je_cache_binary_append_uint32 [基本接口]
向缓存文件追加一个 uint32 值，写入失败返回 false
*/
JE_API bool je_cache_binary_append_uint32(
    je_writing_cache_file_t* file,
    uint32_t val);

/*
je_cache_binary_append_uint64 [基本接口]
向缓存文件追加一个 uint64 值，写入失败返回 false
*/
JE_API bool je_cache_binary_append_uint64(
    je_writing_cache_file_t* file,
    uint64_t val);

/*
je_cache_binary_append_float32 [基本接口]
向缓存文件追加一个 float32 值，写入失败返回 false
*/
JE_API bool je_cache_binary_append_float32(
    je_writing_cache_file_t* file,
    float val);

/*
je_cache_binary_append_float64 [基本接口]
向缓存文件追加一个 float64 值，写入失败返回 false
*/
JE_API bool je_cache_binary_append_float64(
    je_writing_cache_file_t* file,
    double val);
typedef je_writing_cache_file_t je_WritingCacheFile;
typedef je_cache_binary_value_type_t je_CacheBinaryValueType;
typedef je_cache_binary_document_t je_CacheBinaryDocument;
typedef je_cache_binary_list_t je_CacheBinaryList;
typedef je_cache_binary_list_iterator_t je_CacheBinaryListIterator;
typedef je_cache_binary_string_t je_CacheBinaryString;
typedef je_cache_binary_buffer_t je_CacheBinaryBuffer;

JE_FORCE_CAPI_END
