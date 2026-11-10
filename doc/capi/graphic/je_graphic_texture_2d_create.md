# je_graphic_texture_2d_create

## 函数签名

```c
JE_API je_gl_texture_2d_t* je_graphic_texture_2d_create(
    size_t width,
    size_t height,
    je_gl_texture_format_t format,
    /* OPTIONAL: nullptr 表示分配置 0 的像素缓冲 */ const je_gl_texture_pixel_data_t* initial_pixels);
```

## 描述

创建一个指定大小和格式的纹理资源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `width` | `size_t` | 纹理宽度 |
| `height` | `size_t` | 纹理高度 |
| `format` | `je_gl_texture_format_t` | 纹理格式 |
| `initial_pixels` | `const je_gl_texture_pixel_data_t*` | 可选初始像素数据；nullptr 时分配按字节置 0 的像素缓冲 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_texture_2d_t*` | 纹理资源指针（不会返回 `nullptr`） |

## 用法

此函数用于创建空白纹理或渲染目标纹理。

### 纹理格式

常见格式包括：
- `MONO8` / `MONO16` - 单通道
- `RGB` / `RGBA` - 颜色纹理
- `RGBA16` - 高精度颜色
- `DEPTH` - 深度纹理
- `FRAMEBUF` - 帧缓冲附件
- `CUBE` - 立方体纹理

### 示例

```cpp
// 创建 1024x1024 的 RGBA 纹理
je_gl_texture_2d_t* texture = je_graphic_texture_2d_create(1024, 1024, JE_GL_TEXTURE_FORMAT_RGBA);

// 使用纹理...

je_graphic_texture_2d_close(texture);
```

## 注意事项

- 若格式包含 `FLOAT16`、`DEPTH`、`FRAMEBUF` 或有 MSAA 支持，则不创建像素缓冲，像素数据为 `nullptr`
- 若提供 initial_pixels，则按其内容作为初始像素缓冲（逐字节拷贝）；否则分配按字节置 0 的像素缓冲
- 纹理内容在创建后不可变：不支持创建后修改或读取像素
- 如果宽度或高度为 0，将被视为 1
- 使用此接口创建的纹理，其资源路径为 `nullptr`

## 相关接口

- [je_graphic_texture_2d_load](je_graphic_texture_2d_load.md) - 从文件加载纹理
- [je_graphic_texture_2d_close](je_graphic_texture_2d_close.md) - 关闭纹理
