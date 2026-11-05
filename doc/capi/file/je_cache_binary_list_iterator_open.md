# je_cache_binary_list_iterator_open

## 函数签名

```c
JE_API bool je_cache_binary_list_iterator_open(
    const jeecs_cache_binary_list* list,
    jeecs_cache_binary_list_iterator* out_iterator);
```

## 描述

从列表句柄初始化顺序遍历游标

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `list` | `const jeecs_cache_binary_list*` | — |
| `out_iterator` | `jeecs_cache_binary_list_iterator*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 游标指向列表的第一个元素（可能已经是 END）
- 读取接口会移动文件读取位置，不要与顺序读取混用
