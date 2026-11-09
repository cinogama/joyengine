# je_graphic_update

## 函数签名

```c
JE_API bool je_graphic_update(
    je_gl_context_t* thread_handle,
    je_gl_update_sync_mode_t mode,
    je_gl_update_sync_callback_t callback_after_wait_may_null,
    void* callback_param);
```

## 描述

调度图形线程更新一帧。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `thread_handle` | `je_gl_context_t*` | 图形上下文指针 |
| `mode` | `je_gl_update_sync_mode_t` | 更新同步模式 |
| `callback_after_wait_may_null` | `je_gl_update_sync_callback_t` | 等待后的回调函数（可为 `nullptr`） |
| `callback_param` | `void*` | 回调函数参数 |

### 同步模式

| 模式 | 描述 |
|------|------|
| `JE_GL_WAIT_THIS_FRAME_END` | 阻塞直到当前帧渲染完毕。同步更简单，但是绘制时间无法和其他逻辑同步执行 |
| `JE_GL_WAIT_LAST_FRAME_END` | 阻塞直到上一帧渲染完毕（绘制信号发出后立即返回）。更适合 CPU 密集操作，但需要更复杂的机制以保证数据完整性 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 更新是否成功 |

## 用法

此函数用于触发图形线程执行一帧的渲染操作。

### 示例

```cpp
// 简单同步模式
while (running) {
    je_graphic_update(ctx, JE_GL_WAIT_THIS_FRAME_END, nullptr, nullptr);
}

// 带回调的异步模式
void after_wait_callback(void* param) {
    // 在等待完成后执行
}

while (running) {
    je_graphic_update(ctx, JE_GL_WAIT_LAST_FRAME_END, after_wait_callback, my_data);
    
    // 可以在这里执行其他 CPU 工作
    do_cpu_work();
}
```

## 注意事项

- `JE_GL_WAIT_LAST_FRAME_END` 模式需要更复杂的同步机制
- 回调函数会在等待完成后被调用

## 相关接口

- [je_graphic_thread_start](je_graphic_thread_start.md) - 创建图形线程
