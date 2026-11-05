# je_cache_binary_append_int64

## 函数签名

```c
JE_API bool je_cache_binary_append_int64(
    je_writing_cache_file_t* file,
    int64_t val);
```

## 描述

向缓存文件追加一个 int64 值，写入失败返回 false

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `je_writing_cache_file_t*` | — |
| `val` | `int64_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |
