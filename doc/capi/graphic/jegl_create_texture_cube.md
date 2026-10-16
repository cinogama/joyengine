# jegl_create_texture_cube

## 函数签名

```c
JE_API jegl_texture_cube* jegl_create_texture_cube(
    size_t width,
    size_t height,
    jegl_texture_common::format format);
```

## 描述

创建一个指定大小和格式的立方体贴图纹理资源，固定包含 6 个面。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `width` | `size_t` | 每个面的宽度（为 0 时视为 1） |
| `height` | `size_t` | 每个面的高度（为 0 时视为 1） |
| `format` | `jegl_texture_common::format` | 像素格式 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_texture_cube*` | 立方体贴图资源指针，始终非空 |

## 用法

像素数据按 `6 x 宽度 x 高度` 连续排布，面的顺序与图形 API 的
`CUBE_MAP_POSITIVE_X .. NEGATIVE_Z` 顺序一致。

## 注意事项

- 使用 `jegl_close_texture_cube` 释放
- 采样需要着色器声明立方体采样器（当前 woshader 尚未提供）

## 相关接口

- [jegl_create_texture_2d](jegl_create_texture_2d.md) - 创建二维纹理
- [jegl_close_texture_cube](jegl_close_texture_2d.md) - 关闭纹理
