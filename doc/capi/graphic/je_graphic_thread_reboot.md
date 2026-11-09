# je_graphic_thread_reboot

## 函数签名

```c
JE_API void je_graphic_thread_reboot(
    je_gl_context_t* thread_handle,
    const je_gl_interface_config_t* config_may_null);
```

## 描述

以指定的配置请求重启一个图形线程。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `thread_handle` | `je_gl_context_t*` | 图形上下文指针 |
| `config_may_null` | `const je_gl_interface_config_t*` | 新的配置（可为 `nullptr`） |

## 返回值

无返回值。

## 用法

此函数用于在运行时重新配置图形线程，例如切换分辨率、全屏模式等。

### 示例

```cpp
// 不改变配置，只是重启
je_graphic_thread_reboot(ctx, nullptr);

// 切换到全屏模式
je_gl_interface_config_t new_config = current_config;
new_config.m_display_mode = JE_GL_DISPLAY_MODE_FULLSCREEN;
new_config.m_width = 1920;
new_config.m_height = 1080;

je_graphic_thread_reboot(ctx, &new_config);
```

## 注意事项

- 若不需要改变图形配置，请传入 `nullptr`
- 重启过程是异步的，图形线程会在适当时机完成重启

## 相关接口

- [je_graphic_thread_start](je_graphic_thread_start.md) - 创建图形线程
