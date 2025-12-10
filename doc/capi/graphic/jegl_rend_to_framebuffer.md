# jegl_rend_to_framebuffer

## 函数签名

```c
JE_API void jegl_rend_to_framebuffer(
    jegl_frame_buffer* framebuffer,
    size_t x,
    size_t y,
    size_t w,
    size_t h);
```

## 描述

设置渲染目标为指定的帧缓冲区，并设置视口。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `framebuffer` | `jegl_frame_buffer*` | 帧缓冲区资源指针（`nullptr` 表示默认帧缓冲） |
| `x` | `size_t` | 视口起始 X 坐标 |
| `y` | `size_t` | 视口起始 Y 坐标 |
| `w` | `size_t` | 视口宽度 |
| `h` | `size_t` | 视口高度 |

## 返回值

无返回值。

## 用法

此函数用于设置渲染目标和视口区域。

### 示例

```cpp
// 渲染到离屏帧缓冲
jegl_frame_buffer* fbo = jegl_create_framebuf(1024, 1024, formats, 1, true);

jegl_rend_to_framebuffer(fbo, 0, 0, 1024, 1024);
// 绘制场景到 FBO...

// 切换回默认帧缓冲（屏幕）
jegl_rend_to_framebuffer(nullptr, 0, 0, screen_width, screen_height);
// 绘制最终画面...
```

## 注意事项

- 传入 `nullptr` 表示渲染到默认帧缓冲（通常是屏幕）
- 视口坐标系通常以左下角为原点

## 相关接口

- [jegl_create_framebuf](jegl_create_framebuf.md) - 创建帧缓冲
