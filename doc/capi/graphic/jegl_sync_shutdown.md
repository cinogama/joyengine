# jegl_sync_shutdown

## 函数签名

```c
JE_API bool jegl_sync_shutdown(jegl_context* thread, bool isreboot);
```

## 描述

在同步模式下关闭或重启图形上下文。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `thread` | `jegl_context*` | 图形上下文指针 |
| `isreboot` | `bool` | 是否为重启（`true`）还是完全关闭（`false`） |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | `true` 表示确实退出，`false` 表示仍需重新初始化 |

## 用法

根据 `jegl_sync_update` 的返回值，以适当的参数调用此函数以正确完成渲染逻辑。

### 示例

```cpp
jegl_sync_state state = jegl_sync_update(ctx);

if (state == JEGL_SYNC_SHUTDOWN) {
    // 完全关闭
    bool exited = jegl_sync_shutdown(ctx, false);
    // exited 应为 true
} else if (state == JEGL_SYNC_REBOOT) {
    // 重启
    bool exited = jegl_sync_shutdown(ctx, true);
    // exited 应为 false，需要重新初始化
    if (!exited) {
        jegl_sync_init(ctx, true);
    }
}
```

## 注意事项

- 返回值事实上只与 `isreboot` 有关
- 仅在不支持多线程渲染的平台上使用

## 相关接口

- [jegl_sync_init](jegl_sync_init.md) - 同步初始化
- [jegl_sync_update](jegl_sync_update.md) - 同步更新
