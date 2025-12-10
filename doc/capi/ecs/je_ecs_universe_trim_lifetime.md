# je_ecs_universe_trim_lifetime

## 函数签名

```c
JE_API void je_ecs_universe_trim_lifetime(void* universe);
```

## 描述

减少指定 Universe 的生命周期引用计数。当引用计数归零时，请求终止指定 Universe 的运行。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `universe` | `void*` | 指向 Universe 的指针 |

## 返回值

无返回值。

## 用法

此函数用于减少 Universe 的生命周期引用计数，是控制 Universe 退出的标准方式。

### 示例

```cpp
void* universe = je_ecs_universe_create();
// 创建时引用计数为 1

void* world = je_ecs_world_create(universe);
je_ecs_world_set_able(world, true);

// ... 游戏运行中 ...

// 请求退出：减少引用计数
// Universe 会在所有世界关闭后终止运行
je_ecs_universe_trim_lifetime(universe);

// 等待退出完成
je_ecs_universe_loop(universe);
```

## 注意事项

- Universe 创建时自带有一次引用计数
- 当引用计数归零时，会请求销毁 Universe 中的所有世界
- Universe 会在所有世界完全关闭后终止运行
- 如果在退出过程中创建了新世界，Universe 会继续运行直到这些世界也退出
- 不推荐在组件/系统的析构函数中做多余的逻辑操作，析构函数仅用于释放资源

## 相关接口

- [je_ecs_universe_grow_lifetime](je_ecs_universe_grow_lifetime.md) - 增加生命周期引用计数
- [je_ecs_universe_destroy](je_ecs_universe_destroy.md) - 强制销毁 Universe
