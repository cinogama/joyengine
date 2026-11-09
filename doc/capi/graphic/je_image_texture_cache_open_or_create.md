# je_image_texture_cache_open_or_create

## 函数签名

```c
JE_API /* OPTIONAL */ je_file_t* je_image_texture_cache_open_or_create(
    const char* path);
```

## 描述

打开指定图片的纹理缓存 `<path>.je4cache`；缓存缺失或失效时调用内置缓存生成器重建

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | 图片路径 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_file_t*` | 打开的缓存文件；无法读取也无法生成时返回 nullptr |

## 注意事项

- 缓存包含纹理的颜色空间和解码后的 RGBA8 像素数据，加载纹理时可直接使用而无需再解码图片
- 校验码为图片文件与属性源文件 `<path>.je4texture` 内容连续计算的 CRC64，两者任一改变都会使缓存失效；属性源文件是可选的，缺失时颜色空间按默认的 sRGB 处理
- 图片与属性源文件都不存在（如导出的运行时只携带缓存）时跳过校验
- 缓存由 `builtin/script/internal/cache/texture.wo` 生成，文档结构为：`width`、`height`、`color_space`（INT32，0 = sRGB，1 = Linear）与 `pixels`（BUFFER，RGBA8，行序自底向上）
- 返回的句柄由调用方负责关闭（见 je_file_close）

## 相关接口

- [je_graphic_texture_2d_load](je_graphic_texture_2d_load.md) - 从指定路径加载一个二维纹理资源
- [je_cache_file_load](../file/je_cache_file_load.md) - 尝试读取 filepath 对应的缓存文件，将校验缓存文件的格式版本和校验码…
- [je_file_close](../file/je_file_close.md) - 关闭一个文件
