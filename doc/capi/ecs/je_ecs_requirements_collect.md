# je_ecs_requirements_collect

## 函数签名

```c
JE_API je_CollectedRequirements* je_ecs_requirements_collect(
    /* OPTIONAL: If total requirement count is 0 */ const je_ComponentRequirement* requirements,
    size_t view_requiremnt_count,
    size_t other_requiremnts_count);
```

## 描述

把一组组件需求整理为供切片查询使用的紧凑需求对象

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `requirements` | `const je_ComponentRequirement*` | 可选（If total requirement count is 0） requirements 的前 view_requiremnt_count 项作为视图需求，其余作为附加需求 |
| `view_requiremnt_count` | `size_t` | — |
| `other_requiremnts_count` | `size_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_CollectedRequirements*` | 见描述。 |

## 注意事项

- requirements 的前 view_requiremnt_count 项作为视图需求，其余作为附加需求
- requirements 为 nullptr 仅当总需求数为 0

## 相关接口

- [je_ecs_world_collection_update](je_ecs_world_collection_update.md) - 按世界当前的 arch-type 状态刷新需求集合的切片查询缓存
- [je_ecs_world_slice_dependence_query](je_ecs_world_slice_dependence_query.md) - 根据指定的系统实例和切片类型，获取切片查询缓存
