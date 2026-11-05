# je_cache_binary_document_index

## 函数签名

```c
JE_API bool je_cache_binary_document_index(
    const je_cache_binary_document_t* document,
    const char* key,
    je_cache_binary_value_type_t expected_type,
    void* out_value);
```

## 描述

查找文档中指定键对应的值

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `document` | `const je_cache_binary_document_t*` | — |
| `key` | `const char*` | — |
| `expected_type` | `je_cache_binary_value_type_t` | expected_type 指定期望的值类型，实际类型不符时返回 false |
| `out_value` | `void*` | out_value 按 expected_type 解释：BOOL 写入 bool INT8 / INT16 / INT32 / INT64 写入 int8_t / int16_t / int32_t / int64_t UINT8 / UINT16 / UINT32 / UINT64 写入 uint8_t / uint16_t / uint32_t / uint64_t FLOAT32 / FLOAT64 写入 float / double STRING 写入 je_cache_binary_string_t BUFFER 写入 je_cache_binary_buffer_t LIST 写入 je_cache_binary_list_t DOCUMENT 写入 je_cache_binary_document_t |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- expected_type 指定期望的值类型，实际类型不符时返回 false
- out_value 按 expected_type 解释：BOOL 写入 bool INT8 / INT16 / INT32 / INT64 写入 int8_t / int16_t / int32_t / int64_t UINT8 / UINT16 / UINT32 / UINT64 写入 uint8_t / uint16_t / uint32_t / uint64_t FLOAT32 / FLOAT64 写入 float / double STRING 写入 je_cache_binary_string_t BUFFER 写入 je_cache_binary_buffer_t LIST 写入 je_cache_binary_list_t DOCUMENT 写入 je_cache_binary_document_t
- 同一个键出现多次时使用第一个
- 键不存在、类型不符或读取失败时返回 false

## 相关接口

- [je_cache_binary_string_read](je_cache_binary_string_read.md) - 从字符串句柄读取字符串内容
- [je_cache_binary_buffer_read](je_cache_binary_buffer_read.md) - 从缓冲区句柄读取缓冲区内容
- [je_cache_binary_list_index](je_cache_binary_list_index.md) - 获取列表中指定下标的值
