# je_graphic_texture_3d_create

## 函数签名

```c
JE_API je_gl_texture_3d_t* je_graphic_texture_3d_create(
    size_t width,
    size_t height,
    size_t depth,
    je_gl_texture_format_t format,
    /* OPTIONAL: nullptr 表示不创建像素快照 */ const je_gl_texture_pixel_data_t* initial_pixels);
```

## 描述

创建一个指定大小、深度和格式的三维纹理资源。

`initial_pixels` 是一份完整像素快照（行序自底向上），创建时拷贝保存，后端首次
使用纹理时用它完成初始上传；创建后快照只读，运行时修改只能通过
[je_graphic_cmd_upload_texture_3d](je_graphic_cmd_upload_texture_2d.md) 录制区段上传。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `width` | `size_t` | 纹理宽度（为 0 时视为 1） |
| `height` | `size_t` | 纹理高度（为 0 时视为 1） |
| `depth` | `size_t` | 纹理深度方向的纹素数（为 0 时视为 1） |
| `format` | `je_gl_texture_format_t` | 像素格式 |
| `initial_pixels` | `const je_gl_texture_pixel_data_t*` | 创建期像素快照；传 `nullptr` 表示不创建快照 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_texture_3d_t*` | 三维纹理资源指针，始终非空 |

## 用法

像素数据按 `宽度 x 高度 x 深度` 连续排布，行序自底向上。对 `FLOAT16` 格式必须
传 `nullptr`。

## 注意事项

- `initial_pixels` 创建后只读；运行时修改请使用上传命令
- 使用 `je_graphic_texture_3d_close` 释放
- 采样需要着色器声明三维纹理采样器（当前 woshader 尚未提供）

## 相关接口

- [je_graphic_texture_2d_create](je_graphic_texture_2d_create.md) - 创建二维纹理
- [je_graphic_texture_3d_close](je_graphic_texture_2d_close.md) - 关闭纹理
