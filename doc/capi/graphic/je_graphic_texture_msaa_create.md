# je_graphic_texture_msaa_create

## 函数签名

```c
JE_API je_gl_texture_msaa_t* je_graphic_texture_msaa_create(
    size_t width,
    size_t height,
    size_t samples,
    je_gl_texture_format_t format);
```

## 描述

创建一个指定大小、采样数和格式的多重采样二维纹理资源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `width` | `size_t` | 纹理宽度（为 0 时视为 1） |
| `height` | `size_t` | 纹理高度（为 0 时视为 1） |
| `samples` | `size_t` | 每像素采样数（为 0 时视为 1） |
| `format` | `je_gl_texture_format_t` | 像素格式 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_texture_msaa_t*` | 多重采样纹理资源指针，始终非空 |

## 用法

多重采样纹理仅存在于 GPU 侧（用作渲染目标附件），不持有 CPU 像素数据，
因此不支持 CPU 侧像素更新。

## 注意事项

- 使用 `je_graphic_texture_msaa_close` 释放
- OpenGLES3/WebGL2 后端不支持多重采样纹理，创建时将记录错误日志
- 采样需要着色器声明多重采样采样器（当前 woshader 尚未提供）
- 帧缓冲区目前尚不支持附加多重采样纹理（规划中）

## 相关接口

- [je_graphic_texture_2d_create](je_graphic_texture_2d_create.md) - 创建二维纹理
- [je_graphic_texture_msaa_close](je_graphic_texture_2d_close.md) - 关闭纹理
