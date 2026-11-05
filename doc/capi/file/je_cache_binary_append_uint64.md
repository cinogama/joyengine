# je_cache_binary_append_uint64

## 函数签名

```c
JE_API bool je_cache_binary_append_uint64(
    je_writing_cache_file* file,
    uint64_t val);
```

## 描述

向缓存文件追加一个 uint64 值，写入失败返回 false

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `je_writing_cache_file*` | — |
| `val` | `uint64_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |
