# je_image_texture_color_space_get

## 函数签名

```c
JE_API je_image_texture_color_space_t je_image_texture_color_space_get(
    const char* path);
```

## 描述

读取图片纹理的颜色空间（来自属性源文件 <path>.je4texture）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | 图片路径；属性源文件为 `<path>.je4texture` |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_image_texture_color_space_t` | 见描述。 |

## 注意事项

- 属性源文件是 TOML，例如 `color_space = "linear"`；值缺失或非法时为 SRGB
- 引擎按属性源文件的 CRC64 生成/复用 `<path>.je4texture.je4cache`，该缓存可随时删除，删除后按源文件重建
- 属性源文件不存在、缓存无法生成或内容非法时返回 SRGB
- 不检查 path 指向的文件是否为图片

## 相关接口

- [je_image_texture_cache_open_or_create](je_image_texture_cache_open_or_create.md) - 打开或生成图片纹理属性缓存
