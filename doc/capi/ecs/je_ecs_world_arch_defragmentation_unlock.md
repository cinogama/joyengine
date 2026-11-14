# je_ecs_world_arch_defragmentation_unlock

## 函数签名

```c
JE_API void je_ecs_world_arch_defragmentation_unlock(je_game_world_t* world);
```

## 描述

解除 [je_ecs_world_arch_defragmentation_lock](je_ecs_world_arch_defragmentation_lock.md) 对世界碎片整理的锁定。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_game_world_t*` | 目标世界 |

## 返回值

无返回值。

## 注意事项

- 必须与加锁成对，且来自同一线程。
- 解除后世界更新会在后续帧恢复正常执行碎片整理。

## 相关接口

- [je_ecs_world_arch_defragmentation_lock](je_ecs_world_arch_defragmentation_lock.md) - 锁定碎片整理
