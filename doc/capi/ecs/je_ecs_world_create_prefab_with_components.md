# je_ecs_world_create_prefab_with_components

## 函数签名

```cpp
JE_API void je_ecs_world_create_prefab_with_components(
    void* world,
    jeecs::game_entity* out_entity,
    const jeecs::typing::type_info** component_types,
    size_t component_count);
```

## 描述

在指定 World 中创建一个带有指定组件的预制体实体。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| world | void* | World 实例指针 |
| out_entity | jeecs::game_entity* | 输出参数，接收创建的实体 |
| component_types | const jeecs::typing::type_info** | 组件类型数组 |
| component_count | size_t | 组件数量 |

## 返回值

无（通过 out_entity 输出）

## 相关接口

- [je_ecs_world_create_entity_with_components](je_ecs_world_create_entity_with_components.md) - 创建实体
- [je_ecs_world_create_entity_with_prefab](je_ecs_world_create_entity_with_prefab.md) - 从预制体创建实体
