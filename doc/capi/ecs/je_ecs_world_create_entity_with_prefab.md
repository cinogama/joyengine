# je_ecs_world_create_entity_with_prefab

## 函数签名

```cpp
JE_API void je_ecs_world_create_entity_with_prefab(
    void* world,
    jeecs::game_entity* out_entity,
    const jeecs::game_entity* prefab);
```

## 描述

使用预制体在指定 World 中创建一个实体。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| world | void* | World 实例指针 |
| out_entity | jeecs::game_entity* | 输出参数，接收创建的实体 |
| prefab | const jeecs::game_entity* | 预制体实体 |

## 返回值

无（通过 out_entity 输出）

## 相关接口

- [je_ecs_world_create_prefab_with_components](je_ecs_world_create_prefab_with_components.md) - 创建预制体
- [je_ecs_world_create_entity_with_components](je_ecs_world_create_entity_with_components.md) - 直接创建实体
