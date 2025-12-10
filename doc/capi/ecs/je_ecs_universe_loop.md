# je_ecs_universe_loop

## 函数签名

```c
JE_API void je_ecs_universe_loop(void* universe);
```

## 描述

等待指定的 Universe 直到其完全退出运行。这是一个阻塞函数。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `universe` | `void*` | 指向 Universe 的指针 |

## 返回值

无返回值。

## 用法

此函数一般用于 Universe 创建并完成初始状态设定之后，阻塞主线程避免过早退出。

### 示例

```cpp
int main() {
    je_init(0, nullptr);
    
    // 创建 Universe
    void* universe = je_ecs_universe_create();
    
    // 创建世界并初始化
    void* world = je_ecs_world_create(universe);
    // ... 添加系统、创建实体等初始化操作 ...
    je_ecs_world_set_able(world, true);
    
    // 阻塞主线程，等待 Universe 退出
    // Universe 会在所有世界关闭且生命周期计数归零后退出
    je_ecs_universe_loop(universe);
    
    // Universe 已退出，可以安全销毁
    je_ecs_universe_destroy(universe);
    
    je_finish();
    return 0;
}
```

## 注意事项

- 不能对已经终止的 Universe 调用此函数，否则将导致死锁
- 此函数依赖 Universe 的退出回调函数机制
- 在调用此函数前应完成所有初始化操作

## 相关接口

- [je_ecs_universe_create](je_ecs_universe_create.md) - 创建 Universe
- [je_ecs_universe_destroy](je_ecs_universe_destroy.md) - 销毁 Universe
- [je_ecs_universe_register_exit_callback](je_ecs_universe_register_exit_callback.md) - 注册退出回调
