# jegl_sync_update

## 函数签名

```c
JE_API jegl_sync_state jegl_sync_update(jegl_context* thread);
```

## 描述

在同步模式下更新图形的一帧。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `thread` | `jegl_context*` | 图形上下文指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_sync_state` | 同步状态枚举值 |

### 返回状态

| 状态 | 描述 |
|------|------|
| `JEGL_SYNC_COMPLETE` | 正常结束，可继续循环 |
| `JEGL_SYNC_SHUTDOWN` | 绘制将结束，需要以 `isreboot = false` 调用 `jegl_sync_shutdown` |
| `JEGL_SYNC_REBOOT` | 绘制需要重新初始化，需要以 `isreboot = true` 调用 `jegl_sync_shutdown` 后重新初始化 |

## 用法

此函数用于在同步模式下更新一帧图形渲染。

### 示例

```cpp
jegl_context* ctx = /* 获取图形上下文 */;

// 主渲染循环
while (true) {
    jegl_sync_state state = jegl_sync_update(ctx);
    
    switch (state) {
        case JEGL_SYNC_COMPLETE:
            // 正常，继续下一帧
            break;
        case JEGL_SYNC_SHUTDOWN:
            // 结束渲染
            jegl_sync_shutdown(ctx, false);
            return;
        case JEGL_SYNC_REBOOT:
            // 需要重启
            jegl_sync_shutdown(ctx, true);
            jegl_sync_init(ctx, true);
            break;
    }
}
```

## 注意事项

- 根据返回值采取不同的后续操作
- 仅在不支持多线程渲染的平台上使用

## 相关接口

- [jegl_sync_init](jegl_sync_init.md) - 同步初始化
- [jegl_sync_shutdown](jegl_sync_shutdown.md) - 同步关闭
