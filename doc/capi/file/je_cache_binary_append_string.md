# je_cache_binary_append_string

## 函数签名

```c
JE_API bool je_cache_binary_append_string(
    je_writing_cache_file_t* file,
    const char* str);
```

## 描述

向缓存文件追加一个字符串值

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `je_writing_cache_file_t*` | — |
| `str` | `const char*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 写入 uint64 长度和字符串内容，不写入结尾的 '\0'，长度由 strlen 决定
- 写入失败返回 false

## 相关接口

- [je_cache_binary_string_read](je_cache_binary_string_read.md) - 从字符串句柄读取字符串内容
