# jegl_update

## 函数签名

```c
JE_API bool jegl_update(
    jegl_context* thread_handle,
    jegl_update_sync_mode mode,
    jegl_update_sync_callback_t callback_after_wait_may_null,
    void* callback_param);
```

## 描述

调度图形线程更新一帧。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `thread_handle` | `jegl_context*` | 图形上下文指针 |
| `mode` | `jegl_update_sync_mode` | 更新同步模式 |
| `callback_after_wait_may_null` | `jegl_update_sync_callback_t` | 等待后的回调函数（可为 `nullptr`） |
| `callback_param` | `void*` | 回调函数参数 |

### 同步模式

| 模式 | 描述 |
|------|------|
| `JEGL_WAIT_THIS_FRAME_END` | 阻塞直到当前帧渲染完毕。同步更简单，但是绘制时间无法和其他逻辑同步执行 |
| `JEGL_WAIT_LAST_FRAME_END` | 阻塞直到上一帧渲染完毕（绘制信号发出后立即返回）。更适合 CPU 密集操作，但需要更复杂的机制以保证数据完整性 |

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
    jegl_update(ctx, JEGL_WAIT_THIS_FRAME_END, nullptr, nullptr);
}

// 带回调的异步模式
void after_wait_callback(void* param) {
    // 在等待完成后执行
}

while (running) {
    jegl_update(ctx, JEGL_WAIT_LAST_FRAME_END, after_wait_callback, my_data);
    
    // 可以在这里执行其他 CPU 工作
    do_cpu_work();
}
```

## 注意事项

- `JEGL_WAIT_LAST_FRAME_END` 模式需要更复杂的同步机制
- 回调函数会在等待完成后被调用

## 相关接口

- [jegl_start_graphic_thread](jegl_start_graphic_thread.md) - 创建图形线程
