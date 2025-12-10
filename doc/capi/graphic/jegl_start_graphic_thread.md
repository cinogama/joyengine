# jegl_start_graphic_thread

## 函数签名

```c
JE_API jegl_context* jegl_start_graphic_thread(
    jegl_interface_config config,
    void* universe_instance,
    jeecs_api_register_func_t register_func,
    jegl_context::frame_job_func_t frame_rend_work,
    void* arg);
```

## 描述

创建图形线程。指定配置、图形库接口和帧更新函数创建一个图形绘制线程。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `config` | `jegl_interface_config` | 图形接口配置 |
| `universe_instance` | `void*` | Universe 实例指针 |
| `register_func` | `jeecs_api_register_func_t` | API 注册函数 |
| `frame_rend_work` | `jegl_context::frame_job_func_t` | 帧渲染工作函数 |
| `arg` | `void*` | 传递给帧渲染函数的用户参数 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_context*` | 图形上下文指针 |

## 用法

此函数用于创建一个独立的图形渲染线程。

### 配置参数说明

```cpp
struct jegl_interface_config {
    display_mode m_display_mode;  // WINDOWED, FULLSCREEN, BOARDLESS
    bool m_enable_resize;         // 是否允许窗口调整大小
    size_t m_msaa;                // MSAA 采样数（0 表示关闭）
    size_t m_width;               // 窗口宽度（0 使用默认）
    size_t m_height;              // 窗口高度（0 使用默认）
    size_t m_fps;                 // 帧率限制（0 启用垂直同步）
    const char* m_title;          // 窗口标题
    const char* m_icon_path;      // 窗口图标路径
};
```

### 示例

```cpp
jegl_interface_config config = {};
config.m_display_mode = jegl_interface_config::WINDOWED;
config.m_enable_resize = true;
config.m_msaa = 4;
config.m_width = 1280;
config.m_height = 720;
config.m_fps = 60;
config.m_title = "My Game";

jegl_context* ctx = jegl_start_graphic_thread(
    config,
    universe,
    jegl_using_opengl3_apis,
    my_frame_render,
    my_data
);
```

## 注意事项

- 创建出来的图形线程需要使用 `jegl_terminate_graphic_thread` 释放
- 帧更新操作 `jegl_update` 将调用指定的帧更新函数

## 相关接口

- [jegl_terminate_graphic_thread](jegl_terminate_graphic_thread.md) - 终止图形线程
- [jegl_update](jegl_update.md) - 更新图形帧
