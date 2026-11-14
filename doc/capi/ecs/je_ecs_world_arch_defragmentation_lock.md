# je_ecs_world_arch_defragmentation_lock

## 函数签名

```c
JE_API void je_ecs_world_arch_defragmentation_lock(je_game_world_t* world);
```

## 描述

锁定世界的原型碎片整理（chunk 碎片整理）。

碎片整理会把实体在 chunk 之间搬运、回收被搬空的 chunk。其它线程直接遍历世界实体、读取组件期间必须先锁定，
否则已取得的实体句柄/组件地址会因搬运而失配（读到失效/错误的数据）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_game_world_t*` | 目标世界 |

## 返回值

无返回值。

## 用法

```cpp
je_ecs_world_arch_defragmentation_lock(world);
// ...在其它线程遍历实体、读取组件...
je_ecs_world_arch_defragmentation_unlock(world);
```

## 注意事项

- 锁定期间世界更新会**跳过**本轮碎片整理，不会阻塞世界更新；若碎片整理正在进行，本接口等待其完成后才返回。
- 必须与 [je_ecs_world_arch_defragmentation_unlock](je_ecs_world_arch_defragmentation_unlock.md) 成对调用，且解除锁定必须来自同一线程。
- 不支持同一线程重复加锁。
- 锁定只保护 chunk 搬运，不保护游戏逻辑对组件的写入；跨线程读取应只针对不会同时被系统修改的世界（例如未启用的临时世界）。

## 相关接口

- [je_ecs_world_arch_defragmentation_unlock](je_ecs_world_arch_defragmentation_unlock.md) - 解除锁定
