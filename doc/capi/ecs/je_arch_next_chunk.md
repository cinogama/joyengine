# je_arch_next_chunk

## 函数签名

```cpp
JE_API void* je_arch_next_chunk(void* chunk);
```

## 描述

获取指定数据块的下一个数据块。用于遍历原型中的所有数据块。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| chunk | void* | 当前数据块指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| void* | 下一个数据块的指针，如果没有更多数据块则返回 nullptr |

## 用法示例

```cpp
void* chunk = je_arch_get_chunk(archtype);
while (chunk != nullptr) {
    // 处理当前数据块
    
    // 移动到下一个数据块
    chunk = je_arch_next_chunk(chunk);
}
```

## 相关接口

- [je_arch_get_chunk](je_arch_get_chunk.md) - 获取第一个数据块
- [je_arch_entity_meta_addr_in_chunk](je_arch_entity_meta_addr_in_chunk.md) - 获取数据块中的实体元信息
