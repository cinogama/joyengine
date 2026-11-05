# je_ecs_world_collection_update

## 函数签名

```c
JE_API void je_ecs_world_collection_update(
    je_GameWorld* world,
    je_RequirementCollection* collection);
```

## 描述

按世界当前的 arch-type 状态刷新需求集合的切片查询缓存

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_GameWorld*` | — |
| `collection` | `je_RequirementCollection*` | collection 中的 m_collected_requirement 应当已经由 je_ecs_requirements_collect 填充 |

## 返回值

无返回值。

## 注意事项

- 通常配合 je_ecs_world_arch_change_version_get 判断是否需要刷新
- collection 中的 m_collected_requirement 应当已经由 je_ecs_requirements_collect 填充

## 相关接口

- [je_ecs_requirements_collect](je_ecs_requirements_collect.md) - 把一组组件需求整理为供切片查询使用的紧凑需求对象
- [je_ecs_world_arch_change_version_get](je_ecs_world_arch_change_version_get.md) - 返回世界内 arch-type 的变更版本号（单调递增）。每当世界中创建了新…
