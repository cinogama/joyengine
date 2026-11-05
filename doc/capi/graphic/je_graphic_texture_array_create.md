# je_graphic_texture_array_create

## 函数签名

```c
JE_API je_gl_texture_array_t* je_graphic_texture_array_create(
    size_t width,
    size_t height,
    size_t layers,
    je_gl_texture_format_t format);
```

## 描述

创建一个指定大小、层数和格式的纹理数组资源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `width` | `size_t` | 每层纹理宽度（为 0 时视为 1） |
| `height` | `size_t` | 每层纹理高度（为 0 时视为 1） |
| `layers` | `size_t` | 数组中的二维纹理层数（为 0 时视为 1） |
| `format` | `je_gl_texture_format_t` | 像素格式 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_texture_array_t*` | 纹理数组资源指针，始终非空 |

## 用法

像素数据按 `宽度 x 高度 x 层数` 连续排布。着色器中通过
`texture2darray::uniform(pass, sampler)` 声明采样器，并用 `tex2darray(tex, float3(u, v, layer))`
采样指定层。

## 注意事项

- 使用 `je_graphic_texture_array_close` 释放
- 相比图集（atlas）方式，纹理数组从根源上避免了 UV 边缘渗色问题

## 相关接口

- [je_graphic_texture_2d_create](je_graphic_texture_2d_create.md) - 创建二维纹理
- [je_graphic_texture_array_close](je_graphic_texture_2d_close.md) - 关闭纹理
