# je_cache_binary_list_index

## 函数签名

```c
JE_API bool je_cache_binary_list_index(
    const je_cache_binary_list_t* list,
    size_t index,
    je_cache_binary_value_type_t expected_type,
    void* out_value);
```

## 描述

获取列表中指定下标的值

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `list` | `const je_cache_binary_list_t*` | — |
| `index` | `size_t` | — |
| `expected_type` | `je_cache_binary_value_type_t` | expected_type 和 out_value 的约定与 je_cache_binary_document_index 相同 |
| `out_value` | `void*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- expected_type 和 out_value 的约定与 je_cache_binary_document_index 相同
- 下标越界、类型不符或读取失败时返回 false

## 相关接口

- [je_cache_binary_document_index](je_cache_binary_document_index.md) - 查找文档中指定键对应的值
