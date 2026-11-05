# je_graphic_framebuf_create

## 函数签名

```c
JE_API je_gl_frame_buffer_t* je_graphic_framebuf_create(
    size_t width,
    size_t height,
    const je_gl_texture_format_t* color_attachment_formats,
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
| `color_attachment_formats` | `const je_gl_texture_format_t*` | 颜色附件格式数组 |
| `color_attachment_count` | `size_t` | 颜色附件数量 |
| `contain_depth_attachment` | `bool` | 是否包含深度附件 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_frame_buffer_t*` | 帧缓冲区资源指针 |

## 用法

此函数用于创建离屏渲染目标。

### 示例

```cpp
// 创建一个带有两个颜色附件和一个深度附件的帧缓冲
je_gl_texture_format_t formats[] = {
    JE_GL_TEXTURE_FORMAT_RGBA,  // 颜色附件 0
    (je_gl_texture_format_t)(JE_GL_TEXTURE_FORMAT_RGBA | JE_GL_TEXTURE_FORMAT_FLOAT16), // 颜色附件 1 (高精度)
};

je_gl_frame_buffer_t* fbo = je_graphic_framebuf_create(
    1920, 1080,
    formats, 2,
    true    // 包含深度附件
);

// 在命令缓冲区中以该帧缓冲为目标开始一个 pass
// cmd 为当前命令缓冲区，viewport 描述渲染区域
je_graphic_cmd_begin_pass(cmd, fbo, &viewport);

// 使用完毕后释放
je_graphic_framebuf_close(fbo);
```

## 注意事项

- 图形资源创建时自动获取一个引用计数
- 使用此接口创建的帧缓冲，其资源路径为 `nullptr`
- 颜色附件可以作为纹理使用

## 相关接口

- [je_graphic_framebuf_close](je_graphic_framebuf_close.md) - 关闭帧缓冲
- [je_graphic_cmd_begin_pass](je_graphic_cmd_begin_pass.md) - 以帧缓冲为目标开始 pass
