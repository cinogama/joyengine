# je_graphic_texture_3d_create

## 函数签名

```c
JE_API je_gl_texture_3d_t* je_graphic_texture_3d_create(
    size_t width,
    size_t height,
    size_t depth,
    je_gl_texture_format_t format);
```

## 描述

创建一个指定大小、深度和格式的三维纹理资源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `width` | `size_t` | 纹理宽度（为 0 时视为 1） |
| `height` | `size_t` | 纹理高度（为 0 时视为 1） |
| `depth` | `size_t` | 纹理深度方向的纹素数（为 0 时视为 1） |
| `format` | `je_gl_texture_format_t` | 像素格式 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_texture_3d_t*` | 三维纹理资源指针，始终非空 |

## 用法

像素数据按 `宽度 x 高度 x 深度` 连续排布。若格式不含 `FLOAT16` 等特殊标志，
将创建置 0 初始化的 CPU 像素缓冲；直接改写像素后设置
`m_handle.m_modified = true` 可触发重上传。

## 注意事项

- 使用 `je_graphic_texture_3d_close` 释放
- 采样需要着色器声明三维纹理采样器（当前 woshader 尚未提供）

## 相关接口

- [je_graphic_texture_2d_create](je_graphic_texture_2d_create.md) - 创建二维纹理
- [je_graphic_texture_3d_close](je_graphic_texture_2d_close.md) - 关闭纹理
