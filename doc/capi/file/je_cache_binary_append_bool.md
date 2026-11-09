# je_cache_binary_append_bool

## 函数签名

```c
JE_API bool je_cache_binary_append_bool(
    je_writing_cache_file_t* file,
    bool val);
```

## 描述

向缓存文件追加一个 bool 值

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `je_writing_cache_file_t*` | — |
| `val` | `bool` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 固定写入 1 字节，true 写为 1，false 写为 0
- 写入失败返回 false
