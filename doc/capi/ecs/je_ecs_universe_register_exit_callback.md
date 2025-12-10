# je_ecs_universe_register_exit_callback

## 函数签名

```c
JE_API void je_ecs_universe_register_exit_callback(
    void* universe,
    void (*callback)(void*),
    void* arg);
```

## 描述

向指定 Universe 注册退出时的回调函数。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `universe` | `void*` | 指向 Universe 的指针 |
| `callback` | `void (*)(void*)` | 退出时调用的回调函数 |
| `arg` | `void*` | 传递给回调函数的参数 |

## 返回值

无返回值。

## 用法

此函数用于注册 Universe 关闭时需要执行的清理逻辑。回调函数会在 Universe 退出流程中被调用。

### 示例

```cpp
void on_universe_exit(void* userdata) {
    const char* name = (const char*)userdata;
    printf("Universe '%s' is exiting...\n", name);
    // 执行清理操作...
}

void* universe = je_ecs_universe_create();

// 注册退出回调
je_ecs_universe_register_exit_callback(
    universe, 
    on_universe_exit, 
    (void*)"MainUniverse"
);

// ... 使用 Universe ...
```

## 注意事项

- 不能对已经终止的 Universe 注册回调，否则回调函数将不会被执行，并可能导致内存泄漏
- 回调函数会按注册的相反顺序被调用
- 如果回调执行期间有新消息产生，Universe 会继续处理这些消息

## 相关接口

- [je_ecs_universe_create](je_ecs_universe_create.md) - 创建 Universe
- [je_ecs_universe_loop](je_ecs_universe_loop.md) - 等待 Universe 退出
