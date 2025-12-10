# jegl_register_sync_thread_callback

## 函数签名

```c
JE_API void jegl_register_sync_thread_callback(
    jeecs_sync_callback_func_t callback,
    void* arg);
```

## 描述

注册一个同步回调函数，在部分只能在主线程执行图形操作的平台下，此回调用于接收同步调用以创建和操作图形上下文。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `callback` | `jeecs_sync_callback_func_t` | 同步回调函数 |
| `arg` | `void*` | 传递给回调函数的用户参数 |

## 返回值

无返回值。

## 用法

在某些平台（如 WebGL、iOS 等）上，由于图形上下文的限制，不能在独立的渲染线程中执行图形操作。此函数用于注册回调，以便在主线程中手动同步执行图形更新。

### 示例

```cpp
void my_sync_callback(void* ctx, void* arg) {
    jegl_context* thread = (jegl_context*)ctx;
    
    // 初始化
    jegl_sync_init(thread, false);
    
    // 主循环
    while (true) {
        jegl_sync_state state = jegl_sync_update(thread);
        
        if (state == JEGL_SYNC_SHUTDOWN) {
            jegl_sync_shutdown(thread, false);
            break;
        } else if (state == JEGL_SYNC_REBOOT) {
            jegl_sync_shutdown(thread, true);
            jegl_sync_init(thread, true);
        }
    }
}

// 注册回调
jegl_register_sync_thread_callback(my_sync_callback, my_data);
```

## 注意事项

- 必须在 `je_init` 之后注册，引擎初始化会重置回调
- 在支持多线程渲染的平台上通常不需要使用此接口

## 相关接口

- [jegl_sync_init](jegl_sync_init.md) - 同步初始化
- [jegl_sync_update](jegl_sync_update.md) - 同步更新
- [jegl_sync_shutdown](jegl_sync_shutdown.md) - 同步关闭
