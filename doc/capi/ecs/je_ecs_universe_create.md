# je_ecs_universe_create

## 函数签名

```c
JE_API void* je_ecs_universe_create(void);
```

## 描述

创建一个宇宙（Universe），即引擎的全局上下文。引擎允许同时存在多个 Universe，原则上不同 Universe 之间的数据严格隔离并无关。

## 参数

无参数。

## 返回值

| 类型 | 描述 |
|------|------|
| `void*` | 指向新创建的 Universe 的指针 |

## 用法

Universe 是引擎中所有世界、实体和组件的顶层容器。创建 Universe 后会启动一个线程，在循环内执行逻辑更新等操作。

### Universe 生命周期

1. 创建后会创建一个工作线程
2. 线程会在循环内执行逻辑更新
3. 当所有世界关闭后，按顺序执行退出操作：
   - 处理剩余的 Universe 消息
   - 按注册的相反顺序调用退出时回调
   - 如果仍有消息未处理，返回第一步
   - 解除注册所有 Job
4. 完成后，Universe 处于可销毁状态

### 示例

```cpp
int main() {
    je_init(0, nullptr);
    
    // 创建 Universe
    void* universe = je_ecs_universe_create();
    
    // 创建世界
    void* world = je_ecs_world_create(universe);
    je_ecs_world_set_able(world, true);
    
    // 等待 Universe 退出
    je_ecs_universe_loop(universe);
    
    // 销毁 Universe
    je_ecs_universe_destroy(universe);
    
    je_finish();
    return 0;
}
```

## 注意事项

- Universe 创建时自带一次引用计数
- 使用 `je_ecs_universe_trim_lifetime` 减少引用计数
- 引用计数归零时，Universe 会请求终止运行

## 相关接口

- [je_ecs_universe_destroy](je_ecs_universe_destroy.md) - 销毁 Universe
- [je_ecs_universe_loop](je_ecs_universe_loop.md) - 等待 Universe 退出
- [je_ecs_world_create](je_ecs_world_create.md) - 在 Universe 中创建世界
