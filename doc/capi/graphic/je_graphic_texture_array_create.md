# je_graphic_texture_array_create

## 函数签名

```c
JE_API je_gl_texture_array_t* je_graphic_texture_array_create(
    size_t width,
    size_t height,
    size_t layers,
    je_gl_texture_format_t format,
    /* OPTIONAL: nullptr 表示不创建像素快照 */ const je_gl_texture_pixel_data_t* initial_pixels);
```

## 描述

创建一个指定大小、层数和格式的纹理数组资源。

`initial_pixels` 是一份完整像素快照（行序自底向上），创建时拷贝保存，后端首次
使用纹理时用它完成初始上传；创建后快照只读，运行时修改只能通过
[je_graphic_cmd_upload_texture_array](je_graphic_cmd_upload_texture_2d.md) 录制区段上传。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `width` | `size_t` | 每层纹理宽度（为 0 时视为 1） |
| `height` | `size_t` | 每层纹理高度（为 0 时视为 1） |
| `layers` | `size_t` | 数组中的二维纹理层数（为 0 时视为 1） |
| `format` | `je_gl_texture_format_t` | 像素格式 |
| `initial_pixels` | `const je_gl_texture_pixel_data_t*` | 创建期像素快照；传 `nullptr` 表示不创建快照 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_texture_array_t*` | 纹理数组资源指针，始终非空 |

## 用法

像素数据按 `宽度 x 高度 x 层数` 连续排布，行序自底向上。着色器中通过
`texture2darray::uniform(pass, sampler)` 声明采样器，并用 `tex2darray(tex, float3(u, v, layer))`
采样指定层。

## 注意事项

- `initial_pixels` 创建后只读；运行时修改请使用上传命令
- 使用 `je_graphic_texture_array_close` 释放
- 相比图集（atlas）方式，纹理数组从根源上避免了 UV 边缘渗色问题

## 相关接口

- [je_graphic_texture_2d_create](je_graphic_texture_2d_create.md) - 创建二维纹理
- [je_graphic_texture_array_close](je_graphic_texture_2d_close.md) - 关闭纹理
