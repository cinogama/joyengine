# je_cache_binary_append_raw_buffer

## 函数签名

```c
JE_API bool je_cache_binary_append_raw_buffer(
    je_writing_cache_file_t* file,
    const void* buf,
    size_t buflen);
```

## 描述

向缓存文件追加一段没有类型标记的裸字节区

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `je_writing_cache_file_t*` | — |
| `buf` | `const void*` | — |
| `buflen` | `size_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 写入 uint64 长度和 buflen 个字节，布局与 BUFFER 值相同但不写类型标记
- 用于二进制文档之前的裸数据区（例如音频交织 PCM），读取方可以直接
  按偏移访问，不必完整解析文档
- 裸字节区只能写在文档之前：先调用本接口，再调用
  [je_cache_binary_append_document](je_cache_binary_append_document.md)
- 写入失败返回 false

## 相关接口

- [je_cache_binary_append_document](je_cache_binary_append_document.md) - 追加一个 DOCUMENT 标记
- [je_cache_binary_append_buffer](je_cache_binary_append_buffer.md) - 追加一个缓冲区值
