# je_cache_binary_list_iterator_is_end

## 函数签名

```c
JE_API bool je_cache_binary_list_iterator_is_end(
    const jeecs_cache_binary_list_iterator* iterator);
```

## 描述

判断游标是否已经到达列表末尾

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `iterator` | `const jeecs_cache_binary_list_iterator*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 到达 END 返回 true
- 读取失败（越界/截断）返回 false，随后 iterator_read 会失败，调用方据此拒绝损坏的缓存
- 读取不会移动游标的逻辑位置
