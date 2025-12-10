# jegl_reboot_graphic_thread

## 函数签名

```c
JE_API void jegl_reboot_graphic_thread(
    jegl_context* thread_handle,
    const jegl_interface_config* config_may_null);
```

## 描述

以指定的配置请求重启一个图形线程。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `thread_handle` | `jegl_context*` | 图形上下文指针 |
| `config_may_null` | `const jegl_interface_config*` | 新的配置（可为 `nullptr`） |

## 返回值

无返回值。

## 用法

此函数用于在运行时重新配置图形线程，例如切换分辨率、全屏模式等。

### 示例

```cpp
// 不改变配置，只是重启
jegl_reboot_graphic_thread(ctx, nullptr);

// 切换到全屏模式
jegl_interface_config new_config = current_config;
new_config.m_display_mode = jegl_interface_config::FULLSCREEN;
new_config.m_width = 1920;
new_config.m_height = 1080;

jegl_reboot_graphic_thread(ctx, &new_config);
```

## 注意事项

- 若不需要改变图形配置，请传入 `nullptr`
- 重启过程是异步的，图形线程会在适当时机完成重启

## 相关接口

- [jegl_start_graphic_thread](jegl_start_graphic_thread.md) - 创建图形线程
