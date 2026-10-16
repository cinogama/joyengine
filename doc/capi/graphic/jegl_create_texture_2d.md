# jegl_create_texture

## 函数签名

```c
JE_API jegl_texture_2d* jegl_create_texture_2d(
    size_t width,
    size_t height,
    jegl_texture_common::format format);
```

## 描述

创建一个指定大小和格式的纹理资源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `width` | `size_t` | 纹理宽度 |
| `height` | `size_t` | 纹理高度 |
| `format` | `jegl_texture_common::format` | 纹理格式 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_texture_2d*` | 纹理资源指针（不会返回 `nullptr`） |

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
jegl_texture_2d* texture = jegl_create_texture_2d(1024, 1024, jegl_texture::RGBA);

// 使用纹理...

jegl_close_texture_2d(texture);
```

## 注意事项

- 若格式包含 `COLOR16`、`DEPTH`、`FRAMEBUF`、`CUBE` 或有 MSAA 支持，则不创建像素缓冲，像素数据为 `nullptr`
- 若创建像素缓冲，像素缓存按字节置为 0 填充初始化
- 如果宽度或高度为 0，将被视为 1
- 使用此接口创建的纹理，其资源路径为 `nullptr`

## 相关接口

- [jegl_load_texture](jegl_load_texture_2d.md) - 从文件加载纹理
- [jegl_close_texture](jegl_close_texture_2d.md) - 关闭纹理
