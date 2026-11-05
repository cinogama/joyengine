# je_image_texture_color_space_set

## 函数签名

```c
JE_API bool je_image_texture_color_space_set(
    const char* path,
    je_image_texture_color_space_t color_space);
```

## 描述

将图片纹理颜色空间写入 <path>.je4cache

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | — |
| `color_space` | `je_image_texture_color_space_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 缓存带有图片内容的 CRC64；图片之后被修改时，旧缓存会自动失效
- 返回 false 表示缓存写入失败（路径不可写、源文件不可读等）
