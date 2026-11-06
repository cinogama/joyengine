# je_image_texture_color_space_get

## 函数签名

```c
JE_API je_image_texture_color_space_t je_image_texture_color_space_get(
    const char* path);
```

## 描述

读取图片纹理的颜色空间（来自图片纹理缓存 `<path>.je4cache`）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | 图片路径；纹理缓存为 `<path>.je4cache`，属性源文件为 `<path>.je4texture` |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_image_texture_color_space_t` | 见描述。 |

## 注意事项

- 颜色空间的源头是属性源文件 `<path>.je4texture`（TOML，例如 `color_space = "linear"`）；值缺失或非法时为 SRGB
- 图片存在时按需生成或复用纹理缓存（见 je_image_texture_cache_open_or_create）；图片缺失（如导出的运行时只携带缓存）时直接读取缓存并跳过 CRC 校验
- 图片与缓存都不存在、缓存无法生成或内容非法时返回 SRGB
- 缓存缺失时会完整解码图片以生成缓存

## 相关接口

- [je_image_texture_cache_open_or_create](je_image_texture_cache_open_or_create.md) - 打开或生成图片纹理缓存
- [je_graphic_texture_2d_load](je_graphic_texture_2d_load.md) - 加载纹理
