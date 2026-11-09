# je_tilemap_cache_open_or_create

## 函数签名

```c
JE_API /* OPTIONAL */ je_file_t* je_tilemap_cache_open_or_create(const char* path);
```

## 描述

打开指定的瓦片地图缓存文件；缓存缺失或失效时调用内置缓存生成器重建

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_file_t*` | 见描述。 |

## 注意事项

- 返回的句柄由调用方负责关闭（见 je_file_close）；无法生成时返回 nullptr

## 相关接口

- [je_tileset_cache_open_or_create](je_tileset_cache_open_or_create.md) - 打开指定的瓦片集缓存文件；缓存缺失或失效时调用内置缓存生成器重建
- [je_file_close](C:/Cinogama/joyengineecs/doc/capi/file/je_file_close.md) - 关闭一个文件
