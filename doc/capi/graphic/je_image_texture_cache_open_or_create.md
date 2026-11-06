# je_image_texture_cache_open_or_create

## 函数签名

```c
JE_API /* OPTIONAL */ je_file_t* je_image_texture_cache_open_or_create(
    const char* path);
```

## 描述

打开图片 `path` 的纹理缓存 `<path>.je4cache`；缓存缺失或失效时调用内置缓存生成器重建

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | 图片路径；属性源文件为 `<path>.je4texture` |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_file_t*` | 见描述。 |

## 缓存内容

缓存由图片与可选的属性源文件 `<path>.je4texture` 共同生成（生成器为 `builtin/script/internal/cache/texture.wo`），文件头之后是一个二进制缓存文档：

| 键 | 类型 | 描述 |
|----|------|------|
| `color_space` | INT32 | 颜色空间（0 = sRGB，1 = Linear），来自属性源文件 |
| `width` | INT32 | 图片宽度 |
| `height` | INT32 | 图片高度 |
| `pixels` | BUFFER | `width * height * 4` 字节 RGBA8 像素，行序自下而上 |

## 注意事项

- 校验码为图片与属性源文件内容依次拼接后的 crc64（见 je_cache_file_crc64）；修改图片或属性源文件都会使缓存失效
- 图片不可读时跳过 CRC 校验，使导出的运行时可以只携带缓存
- 返回的句柄由调用方负责关闭（见 je_file_close）；无法生成时返回 nullptr

## 相关接口

- [je_image_texture_color_space_get](je_image_texture_color_space_get.md) - 读取图片纹理的颜色空间
- [je_graphic_texture_2d_load](je_graphic_texture_2d_load.md) - 加载纹理
- [je_cache_file_crc64](../file/je_cache_file_crc64.md) - 计算文件内容的 crc64 校验值
- [je_cache_file_load](../file/je_cache_file_load.md) - 尝试读取 filepath 对应的缓存文件，将校验缓存文件的格式版本和校验码…
- [je_file_close](../file/je_file_close.md) - 关闭一个文件
