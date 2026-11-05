# je_tilemap_map_create

## 函数签名

```c
JE_API /* OPTIONAL: If size is invalid */ je_tilemap_document_t* je_tilemap_map_create(int32_t w, int32_t h, int32_t tile_px);
```

## 描述

新建一个未保存的地图文档（单图层），返回文档实例指针

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `w` | `int32_t` | — |
| `h` | `int32_t` | — |
| `tile_px` | `int32_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_tilemap_document_t*` | 见描述。 |

## 注意事项

- 尺寸不合法（<=0 或超过 65536）时返回 nullptr
- 调用方持有 1 个文档引用（见 je_tilemap_map_unload）
