# je_tilemap_map_path_bind

## 函数签名

```c
JE_API bool je_tilemap_map_path_bind(
    je_TilemapDocument* map, const char* path);
```

## 描述

编辑器已把地图的 TOML 源写入 path（.je4tilemap）后，把该路径登记为文档路径

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `path` | `const char*` | path 支持 @/ 与 !/ 前缀（与 je_tilemap_map_open 一致） |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 引擎不写盘、不生成/重建缓存：<path>.je4cache 在下次加载时按源文件 CRC64 自动失效并重建
- 解除本文档的旧路径绑定并顶替 path 上的既有绑定
- 文档须由调用方持有外部引用，否则返回 false
- path 支持 @/ 与 !/ 前缀（与 je_tilemap_map_open 一致）

## 相关接口

- [je_tilemap_map_open](je_tilemap_map_open.md) - 打开（或取进程内缓存）指定路径的地图文档，返回文档实例指针
