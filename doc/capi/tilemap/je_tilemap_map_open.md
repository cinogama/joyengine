# je_tilemap_map_open

## 函数签名

```c
JE_API /* OPTIONAL: If path cannot be opened */ je_tilemap_document_t* je_tilemap_map_open(const char* path);
```

## 描述

打开（或取进程内缓存）指定路径的地图文档，返回文档实例指针

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_tilemap_document_t*` | 见描述。 |

## 注意事项

- 路径不存在或无法解析时返回 nullptr
- 每次成功调用为调用方持有 1 个文档引用（见 je_tilemap_map_unload）

## 相关接口

- [je_tilemap_map_keep](je_tilemap_map_keep.md) - 地图文档引用计数 +1（文档一经加载默认长期驻留，引用计数是唯一的卸载途径）…
