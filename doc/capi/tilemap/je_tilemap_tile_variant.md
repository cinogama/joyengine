# je_tilemap_tile_variant

## 函数签名

```c
JE_API bool je_tilemap_tile_variant(je_TilesetDocument* tileset, je_TileId tile_id,
    int32_t index, je_TileId* out_variant);
```

## 描述

按 47 变体序取指定 id 瓦片的显式变体瓦片 id

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_TilesetDocument*` | — |
| `tile_id` | `je_TileId` | — |
| `index` | `int32_t` | index 越界或未配置变体返回 false |
| `out_variant` | `je_TileId*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- index 越界或未配置变体返回 false

## 相关接口

- [je_tilemap_tile_variant_count](je_tilemap_tile_variant_count.md) - 取指定 id 瓦片的 blob47 显式变体数（0 = 按锚点起行优先连续取…
