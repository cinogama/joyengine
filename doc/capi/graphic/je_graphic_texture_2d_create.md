# je_graphic_texture_2d_create

## 函数签名

```c
JE_API je_gl_texture_2d_t* je_graphic_texture_2d_create(
    size_t width,
    size_t height,
    je_gl_texture_format_t format,
    /* OPTIONAL: nullptr 表示不创建像素快照 */ const je_gl_texture_pixel_data_t* initial_pixels);
```

## 描述

创建一个指定大小和格式的纹理资源。

`initial_pixels` 是一份完整像素快照（行序自底向上），创建时拷贝保存；后端在纹理
首次被使用时用它完成初始上传。创建完成后快照只读，运行时修改纹理只能通过
[je_graphic_cmd_upload_texture_2d](je_graphic_cmd_upload_texture_2d.md) 录制区段上传。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `width` | `size_t` | 纹理宽度 |
| `height` | `size_t` | 纹理高度 |
| `format` | `je_gl_texture_format_t` | 纹理格式 |
| `initial_pixels` | `const je_gl_texture_pixel_data_t*` | 创建期像素快照；传 `nullptr` 表示不创建快照 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_texture_2d_t*` | 纹理资源指针（不会返回 `nullptr`） |

## 用法

### 纹理格式

常见格式包括：
- `JE_GL_TEXTURE_FORMAT_MONO` - 单通道
- `JE_GL_TEXTURE_FORMAT_RGBA` - 颜色纹理
- `JE_GL_TEXTURE_FORMAT_FLOAT16` - 半精度浮点
- `JE_GL_TEXTURE_FORMAT_DEPTH` - 深度纹理
- `JE_GL_TEXTURE_FORMAT_FRAMEBUF` - 帧缓冲附件
- `JE_GL_TEXTURE_FORMAT_SRGB` - 采样时按 sRGB 解码
- `JE_GL_TEXTURE_FORMAT_BC1` / `JE_GL_TEXTURE_FORMAT_BC3` - 4x4 块压缩格式（8 / 16 字节每块，后者保留 8 位 alpha）

非压缩格式的 `initial_pixels` 长度为 宽 * 高 * (COLOR_DEPTH_MASK 字节数)；块压缩
格式为 ceil(宽/4) * ceil(高/4) * 每块字节数。块压缩纹理主要用于图片加载得到的只读
纹理（不支持 [je_graphic_cmd_upload_texture_2d](je_graphic_cmd_upload_texture_2d.md)
区段上传）。

### 示例

```cpp
// 创建 1024x1024 的 RGBA 纹理，并给定初始像素
std::vector<je_gl_texture_pixel_data_t> pixels(1024 * 1024 * 4, 0xFF);
je_gl_texture_2d_t* texture = je_graphic_texture_2d_create(
    1024, 1024, JE_GL_TEXTURE_FORMAT_RGBA, pixels.data());

// 使用纹理...

je_graphic_texture_2d_close(texture);
```

## 注意事项

- `initial_pixels` 只在后端首次初始化（以及图形线程重建）时读取；创建后不再允许改写。
- 对 `FLOAT16`、`DEPTH`、`FRAMEBUF` 格式必须传 `nullptr`。
- 块压缩格式（`BC1`/`BC3`）的 `initial_pixels` 是压缩后的块数据，不是 RGBA8 像素；块压缩纹理创建后不支持区段上传。
- 空纹理（`initial_pixels == nullptr`）在 GPU 侧未初始化，内容由后续区段上传填充。
- 如果宽度或高度为 0，将被视为 1
- 使用此接口创建的纹理，其资源路径为 `nullptr`

## 相关接口

- [je_graphic_texture_2d_load](je_graphic_texture_2d_load.md) - 从文件加载纹理
- [je_graphic_cmd_upload_texture_2d](je_graphic_cmd_upload_texture_2d.md) - 持久区段上传
- [je_graphic_texture_2d_close](je_graphic_texture_2d_close.md) - 关闭纹理
