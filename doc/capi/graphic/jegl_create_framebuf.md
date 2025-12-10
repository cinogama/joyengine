# jegl_create_framebuf

## 函数签名

```c
JE_API jegl_frame_buffer* jegl_create_framebuf(
    size_t width,
    size_t height,
    const jegl_texture::format* color_attachment_formats,
    size_t color_attachment_count,
    bool contain_depth_attachment);
```

## 描述

使用指定的附件配置创建一个帧缓冲区资源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `width` | `size_t` | 帧缓冲区宽度 |
| `height` | `size_t` | 帧缓冲区高度 |
| `color_attachment_formats` | `const jegl_texture::format*` | 颜色附件格式数组 |
| `color_attachment_count` | `size_t` | 颜色附件数量 |
| `contain_depth_attachment` | `bool` | 是否包含深度附件 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_frame_buffer*` | 帧缓冲区资源指针 |

## 用法

此函数用于创建离屏渲染目标。

### 示例

```cpp
// 创建一个带有两个颜色附件和一个深度附件的帧缓冲
jegl_texture::format formats[] = {
    jegl_texture::RGBA,     // 颜色附件 0
    jegl_texture::RGBA16,   // 颜色附件 1 (高精度)
};

jegl_frame_buffer* fbo = jegl_create_framebuf(
    1920, 1080,
    formats, 2,
    true    // 包含深度附件
);

// 渲染到帧缓冲
jegl_rend_to_framebuffer(fbo, 0, 0, 1920, 1080);

// 使用完毕后释放
jegl_close_framebuf(fbo);
```

## 注意事项

- 图形资源创建时自动获取一个引用计数
- 使用此接口创建的帧缓冲，其资源路径为 `nullptr`
- 颜色附件可以作为纹理使用

## 相关接口

- [jegl_close_framebuf](jegl_close_framebuf.md) - 关闭帧缓冲
- [jegl_rend_to_framebuffer](jegl_rend_to_framebuffer.md) - 渲染到帧缓冲
