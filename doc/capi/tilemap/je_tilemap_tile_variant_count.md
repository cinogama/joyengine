# je_tilemap_tile_variant_count

## 函数签名

```c
JE_API int32_t je_tilemap_tile_variant_count(je_tileset_document_t* tileset, je_tile_id_t tile_id);
```

## 描述

取指定 id 瓦片的 blob47 显式变体数（0 = 按锚点起行优先连续取块的默认布局）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `tile_id` | `je_tile_id_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |

## 相关接口

- [je_tilemap_tile_variant](je_tilemap_tile_variant.md) - 按 47 变体序取指定 id 瓦片的显式变体瓦片 id
