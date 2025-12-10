# je_ecs_world_set_able

## 函数签名

```c
JE_API void je_ecs_world_set_able(void* world, bool enable);
```

## 描述

设置世界是否被激活。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `void*` | 指向世界的指针 |
| `enable` | `bool` | `true` 激活世界，`false` 暂停世界 |

## 返回值

无返回值。

## 用法

此函数用于控制世界的激活状态，影响世界中的系统和实体更新。

### 示例

```cpp
void* universe = je_ecs_universe_create();
void* world = je_ecs_world_create(universe);

// 添加系统和创建实体...

// 激活世界，开始更新循环
je_ecs_world_set_able(world, true);

// ... 运行一段时间后 ...

// 暂停世界
je_ecs_world_set_able(world, false);

// 恢复世界
je_ecs_world_set_able(world, true);
```

## 注意事项

- 若世界未激活，实体组件系统更新将被暂停
- 未激活的世界仅响应销毁请求和激活请求
- 世界在激活/取消激活时，所有系统的 `OnEnable`/`OnDisable` 回调会被执行
- 如果系统实例创建时世界尚未激活，系统的 `OnEnable` 回调不会在创建时执行，而是在世界激活时执行

## 相关接口

- [je_ecs_world_create](je_ecs_world_create.md) - 创建世界
- [je_ecs_world_destroy](je_ecs_world_destroy.md) - 销毁世界
