# je_graphic_sync_init

## 函数签名

```c
JE_API void je_graphic_sync_init(jegl_context* thread, bool isreboot);
```

## 描述

在同步模式下初始化图形上下文。用于图形在首次启动或重启时执行图形任务的初始化操作。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `thread` | `jegl_context*` | 图形上下文指针 |
| `isreboot` | `bool` | 是否为重启初始化 |

## 返回值

无返回值。

## 用法

一些平台下，由于上下文等限制，不能创建独立的渲染线程。对于这种情况，需要手动执行 `je_graphic_sync_init`、`je_graphic_sync_update` 和 `je_graphic_sync_shutdown`。

### 示例

```cpp
jegl_context* ctx = /* 获取图形上下文 */;

// 首次初始化
je_graphic_sync_init(ctx, false);

// 主循环
while (running) {
    jegl_sync_state state = je_graphic_sync_update(ctx);
    
    if (state == JEGL_SYNC_REBOOT) {
        // 需要重启
        je_graphic_sync_shutdown(ctx, true);
        je_graphic_sync_init(ctx, true);  // 重启初始化
    } else if (state == JEGL_SYNC_SHUTDOWN) {
        je_graphic_sync_shutdown(ctx, false);
        break;
    }
}
```

## 注意事项

- 初始化完成后，循环执行 `je_graphic_sync_update` 直到其返回 `JEGL_SYNC_SHUTDOWN` 或 `JEGL_SYNC_REBOOT`
- 仅在不支持多线程渲染的平台上使用

## 相关接口

- [je_graphic_sync_thread_callback_register](je_graphic_sync_thread_callback_register.md) - 注册同步回调
- [je_graphic_sync_update](je_graphic_sync_update.md) - 同步更新
- [je_graphic_sync_shutdown](je_graphic_sync_shutdown.md) - 同步关闭
