# je_ecs_world_arch_change_version_get

## 函数签名

```c
JE_API size_t je_ecs_world_arch_change_version_get(je_GameWorld* world);
```

## 描述

返回世界内 arch-type 的变更版本号（单调递增）。每当世界中创建了新的 arch-type 时该值递增。此函数为非消费式读取：调用不会影响 ecs_world::update 对内部系统切片缓存的刷新逻辑。外部持久化的 je_RequirementCollection 可通过比较两次观测值来判断是否需要刷新（调用 je_ecs_world_collection_update）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_GameWorld*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `size_t` | 见描述。 |
