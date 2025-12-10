# je_arch_entity_meta_addr_in_chunk

## 函数签名

```cpp
JE_API const jeecs::game_entity::meta* je_arch_entity_meta_addr_in_chunk(void* chunk);
```

## 描述

获取指定数据块中实体元信息数组的起始地址。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| chunk | void* | 数据块指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| const jeecs::game_entity::meta* | 实体元信息数组的起始指针 |

## 用法示例

```cpp
void* chunk = je_arch_get_chunk(archtype);
const jeecs::game_entity::meta* metas = je_arch_entity_meta_addr_in_chunk(chunk);

// 遍历数据块中的实体元信息
// ...
```

## 相关接口

- [je_arch_get_chunk](je_arch_get_chunk.md) - 获取第一个数据块
- [je_arch_next_chunk](je_arch_next_chunk.md) - 获取下一个数据块
