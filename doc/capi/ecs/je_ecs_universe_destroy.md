# je_ecs_universe_destroy

## 函数签名

```c
JE_API void je_ecs_universe_destroy(void* universe);
```

## 描述

销毁一个 Universe。此函数会阻塞直到 Universe 完全销毁。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `universe` | `void*` | 指向需要销毁的 Universe 的指针 |

## 返回值

无返回值。

## 用法

此函数用于强制销毁一个 Universe，无视其生命周期计数。

### 示例

```cpp
void* universe = je_ecs_universe_create();

// 使用 Universe...
void* world = je_ecs_world_create(universe);
// ...

// 销毁 Universe（会阻塞直到完全销毁）
je_ecs_universe_destroy(universe);
```

## 注意事项

- 此操作无视 Universe 的生命周期计数，立即请求终止运行
- 函数会阻塞直到 Universe 完全销毁
- 销毁前确保不再需要该 Universe 中的任何资源

## 相关接口

- [je_ecs_universe_create](je_ecs_universe_create.md) - 创建 Universe
- [je_ecs_universe_loop](je_ecs_universe_loop.md) - 等待 Universe 退出
- [je_ecs_universe_trim_lifetime](je_ecs_universe_trim_lifetime.md) - 减少生命周期引用计数
