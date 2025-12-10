# je_ecs_world_of_entity

## 函数签名

```cpp
JE_API void* je_ecs_world_of_entity(const jeecs::game_entity* entity);
```

## 描述

获取实体所属的 World。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| entity | const jeecs::game_entity* | 实体指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| void* | 实体所属的 World 实例指针 |

## 相关接口

- [je_ecs_world_in_universe](je_ecs_world_in_universe.md) - 获取 World 所属的 Universe
