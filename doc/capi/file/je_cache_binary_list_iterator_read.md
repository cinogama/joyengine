# je_cache_binary_list_iterator_read

## 函数签名

```c
JE_API bool je_cache_binary_list_iterator_read(
    const je_cache_binary_list_iterator_t* iterator,
    je_cache_binary_value_type_t expected_type,
    void* out_value);
```

## 描述

读取游标当前元素

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `iterator` | `const je_cache_binary_list_iterator_t*` | — |
| `expected_type` | `je_cache_binary_value_type_t` | expected_type 和 out_value 的约定与 je_cache_binary_document_index 相同 |
| `out_value` | `void*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- expected_type 和 out_value 的约定与 je_cache_binary_document_index 相同
- 不移动游标；类型不符或读取失败时返回 false

## 相关接口

- [je_cache_binary_list_iterator_advance](je_cache_binary_list_iterator_advance.md) - 把游标移动到下一个元素
