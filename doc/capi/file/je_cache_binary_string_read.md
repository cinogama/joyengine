# je_cache_binary_string_read

## 函数签名

```c
JE_API bool je_cache_binary_string_read(
    /* OPTIONAL: If *inout_dst_len == 0 */ char* dst,
    size_t* inout_dst_len,
    const je_cache_binary_string_t* str);
```

## 描述

从字符串句柄读取字符串内容

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `dst` | `char*` | 可选（If *inout_dst_len == 0） dst == nullptr 时 inout_dst_len 必须为 0，此时仅查询字符串长度 |
| `inout_dst_len` | `size_t*` | inout_dst_len 传入 dst 的容量，成功返回时被改写为字符串的实际长度 |
| `str` | `const je_cache_binary_string_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- inout_dst_len 传入 dst 的容量，成功返回时被改写为字符串的实际长度
- 容量不足时只复制前 inout_dst_len 个字节；复制后仍有空位时才写入 '\0'
- dst == nullptr 时 inout_dst_len 必须为 0，此时仅查询字符串长度
- 实际长度可能大于容量，调用方可据此重新分配后再次读取
- 读取失败返回 false

## 相关接口

- [je_cache_binary_document_index](je_cache_binary_document_index.md) - 查找文档中指定键对应的值
- [je_cache_binary_list_index](je_cache_binary_list_index.md) - 获取列表中指定下标的值
