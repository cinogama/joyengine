# jegl_create_texture_array

## 函数签名

```c
JE_API jegl_texture_array* jegl_create_texture_array(
    size_t width,
    size_t height,
    size_t layers,
    jegl_texture_common::format format);
```

## 描述

创建一个指定大小、层数和格式的纹理数组资源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `width` | `size_t` | 每层纹理宽度（为 0 时视为 1） |
| `height` | `size_t` | 每层纹理高度（为 0 时视为 1） |
| `layers` | `size_t` | 数组中的二维纹理层数（为 0 时视为 1） |
| `format` | `jegl_texture_common::format` | 像素格式 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_texture_array*` | 纹理数组资源指针，始终非空 |

## 用法

像素数据按 `宽度 x 高度 x 层数` 连续排布。着色器中通过
`texture2darray::uniform(pass, sampler)` 声明采样器，并用 `tex2darray(tex, float3(u, v, layer))`
采样指定层。

## 注意事项

- 使用 `jegl_close_texture_array` 释放
- 相比图集（atlas）方式，纹理数组从根源上避免了 UV 边缘渗色问题

## 相关接口

- [jegl_create_texture_2d](jegl_create_texture_2d.md) - 创建二维纹理
- [jegl_close_texture_array](jegl_close_texture_2d.md) - 关闭纹理
