# je_cache_binary_append_buffer

## 函数签名

```c
JE_API bool je_cache_binary_append_buffer(
    je_writing_cache_file* file,
    const void* buf,
    size_t buflen);
```

## 描述

向缓存文件追加一个缓冲区值

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `je_writing_cache_file*` | — |
| `buf` | `const void*` | — |
| `buflen` | `size_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 写入 uint64 长度和 buflen 个字节
- 写入失败返回 false

## 相关接口

- [je_cache_binary_buffer_read](je_cache_binary_buffer_read.md) - 从缓冲区句柄读取缓冲区内容
