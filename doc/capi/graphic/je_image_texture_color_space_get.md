# je_image_texture_color_space_get

## 函数签名

```c
JE_API je_image_texture_color_space_t je_image_texture_color_space_get(
    const char* path);
```

## 描述

读取图片纹理属性缓存（<path>.je4cache）中记录的颜色空间

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_image_texture_color_space_t` | 见描述。 |

## 注意事项

- 缓存不存在、校验失败（图片已改动）或内容非法时返回 SRGB
- 不检查 path 指向的文件是否为图片
