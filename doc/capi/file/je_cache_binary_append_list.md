# je_cache_binary_append_list

## 函数签名

```c
JE_API bool je_cache_binary_append_list(je_writing_cache_file_t* file);
```

## 描述

向缓存文件追加一个 LIST 标记

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `je_writing_cache_file_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 之后依次写入各元素，最后由 je_cache_binary_append_end 结束
- 写入失败返回 false

## 相关接口

- [je_cache_binary_append_end](je_cache_binary_append_end.md) - 向缓存文件追加一个 END 标记，结束最近一个尚未结束的列表或文档
