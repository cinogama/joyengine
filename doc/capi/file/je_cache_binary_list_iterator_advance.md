# je_cache_binary_list_iterator_advance

## 函数签名

```c
JE_API bool je_cache_binary_list_iterator_advance(
    jeecs_cache_binary_list_iterator* inout_iterator);
```

## 描述

把游标移动到下一个元素

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `inout_iterator` | `jeecs_cache_binary_list_iterator*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 已到达 END 或读取失败时返回 false

## 相关接口

- [je_cache_binary_list_iterator_read](je_cache_binary_list_iterator_read.md) - 读取游标当前元素
