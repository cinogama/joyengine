# je_ecs_world_slice_dependence_query

## 函数签名

```cpp
JE_API bool je_ecs_world_slice_dependence_query(
    je_GameWorld* world,
    jeecs::game_system* system_instance,
    je_TypeHash slice_type_hash,
    je_RequirementCollection** out_collection);
```

## 描述

根据指定的系统实例和切片类型，获取对应的切片查询缓存。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_GameWorld*` | World 实例指针 |
| `system_instance` | `jeecs::game_system*` | 系统实例 |
| `slice_type_hash` | `je_TypeHash` | 切片类型的哈希 |
| `out_collection` | `je_RequirementCollection**` | 输出切片查询缓存 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 找到缓存返回 true；否则返回 false |

## 注意事项

- 返回 false 时，应当由调用方初始化切片查询缓存的 requirements 字段并执行更新
- 获取到的查询缓存仅在当前帧有效，不应当被持久化保存

## 相关接口

- [je_ecs_requirements_collect](je_ecs_requirements_collect.md) - 收集切片查询需求
- [je_ecs_world_collection_update](je_ecs_world_collection_update.md) - 刷新需求集合的切片查询缓存
