# je_image_texture_cache_open_or_create

## 函数签名

```c
JE_API /* OPTIONAL */ je_file_t* je_image_texture_cache_open_or_create(
    const char* path);
```

## 描述

打开指定的图片纹理属性缓存；缓存缺失或失效时调用内置缓存生成器重建

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | 属性源文件路径（`<图片路径>.je4texture`），不是图片路径 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_file_t*` | 见描述。 |

## 注意事项

- 返回的句柄由调用方负责关闭（见 je_file_close）；无法生成时返回 nullptr

## 相关接口

- [je_image_texture_color_space_get](je_image_texture_color_space_get.md) - 读取图片纹理的颜色空间
- [je_cache_file_load](C:/Cinogama/joyengineecs/doc/capi/file/je_cache_file_load.md) - 尝试读取 filepath 对应的缓存文件，将校验缓存文件的格式版本和校验码…
- [je_file_close](C:/Cinogama/joyengineecs/doc/capi/file/je_file_close.md) - 关闭一个文件
