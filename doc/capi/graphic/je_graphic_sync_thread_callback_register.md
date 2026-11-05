# je_graphic_sync_thread_callback_register

## 函数签名

```c
JE_API void je_graphic_sync_thread_callback_register(
    je_graphic_sync_callback_func_t callback,
    void* arg);
```

## 描述

注册一个同步回调函数，在部分只能在主线程执行图形操作的平台下，此回调用于接收同步调用以创建和操作图形上下文。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `callback` | `je_graphic_sync_callback_func_t` | 同步回调函数 |
| `arg` | `void*` | 传递给回调函数的用户参数 |

## 返回值

无返回值。

## 用法

在某些平台（如 WebGL、iOS 等）上，由于图形上下文的限制，不能在独立的渲染线程中执行图形操作。此函数用于注册回调，以便在主线程中手动同步执行图形更新。

### 示例

```cpp
void my_sync_callback(void* ctx, void* arg) {
    je_gl_context_t* thread = (je_gl_context_t*)ctx;
    
    // 初始化
    je_graphic_sync_init(thread, false);
    
    // 主循环
    while (true) {
        je_gl_sync_state_t state = je_graphic_sync_update(thread);
        
        if (state == JE_GL_SYNC_SHUTDOWN) {
            je_graphic_sync_shutdown(thread, false);
            break;
        } else if (state == JE_GL_SYNC_REBOOT) {
            je_graphic_sync_shutdown(thread, true);
            je_graphic_sync_init(thread, true);
        }
    }
}

// 注册回调
je_graphic_sync_thread_callback_register(my_sync_callback, my_data);
```

## 注意事项

- 必须在 `je_core_init` 之后注册，引擎初始化会重置回调
- 在支持多线程渲染的平台上通常不需要使用此接口

## 相关接口

- [je_graphic_sync_init](je_graphic_sync_init.md) - 同步初始化
- [je_graphic_sync_update](je_graphic_sync_update.md) - 同步更新
- [je_graphic_sync_shutdown](je_graphic_sync_shutdown.md) - 同步关闭
