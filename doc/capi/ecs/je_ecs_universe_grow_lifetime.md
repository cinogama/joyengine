# je_ecs_universe_grow_lifetime

## 函数签名

```c
JE_API void je_ecs_universe_grow_lifetime(void* universe);
```

## 描述

延长指定 Universe 的生命周期，防止其自动销毁。每次调用都会增加 Universe 的引用计数。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `universe` | `void*` | 指向 Universe 的指针 |

## 返回值

无返回值。

## 用法

此函数用于在需要手动管理 Universe 生命周期时使用。

### 示例

```cpp
void* universe = je_ecs_universe_create();
// Universe 创建时引用计数为 1

// 增加引用计数，现在为 2
je_ecs_universe_grow_lifetime(universe);

// 减少引用计数，现在为 1
je_ecs_universe_trim_lifetime(universe);

// 再次减少，引用计数归零，Universe 开始退出流程
je_ecs_universe_trim_lifetime(universe);
```

## 注意事项

- Universe 创建时自带有一次引用计数
- 需要与 `je_ecs_universe_trim_lifetime` 配对使用
- 引用计数归零时 Universe 会开始退出流程

## 相关接口

- [je_ecs_universe_trim_lifetime](je_ecs_universe_trim_lifetime.md) - 减少生命周期引用计数
- [je_ecs_universe_create](je_ecs_universe_create.md) - 创建 Universe
