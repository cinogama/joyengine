# je_arch_get_chunk

## 函数签名

```cpp
JE_API void* je_arch_get_chunk(void* archtype);
```

## 描述

获取指定原型（archetype）的第一个数据块（chunk）。原型是 ECS 中用于存储具有相同组件组合的实体的数据结构。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| archtype | void* | 原型指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| void* | 第一个数据块的指针，如果没有数据块则返回 nullptr |

## 用法示例

```cpp
void* chunk = je_arch_get_chunk(archtype);
while (chunk != nullptr) {
    // 处理数据块中的实体
    const jeecs::game_entity::meta* meta = je_arch_entity_meta_addr_in_chunk(chunk);
    
    // 获取下一个数据块
    chunk = je_arch_next_chunk(chunk);
}
```

## 相关接口

- [je_arch_next_chunk](je_arch_next_chunk.md) - 获取下一个数据块
- [je_arch_entity_meta_addr_in_chunk](je_arch_entity_meta_addr_in_chunk.md) - 获取数据块中的实体元信息
