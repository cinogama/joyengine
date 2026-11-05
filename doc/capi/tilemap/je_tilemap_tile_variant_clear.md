# je_tilemap_tile_variant_clear

## 函数签名

```c
JE_API bool je_tilemap_tile_variant_clear(je_tileset_document_t* tileset, je_tile_id_t tile_id);
```

## 描述

清空指定 id 瓦片的 blob47 显式变体表；空表时按锚点行优先扫描 47 块

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `tile_id` | `je_tile_id_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 相关接口

- [je_tilemap_tile_variant_add](je_tilemap_tile_variant_add.md) - 向指定 id 瓦片的 blob47 显式变体表追加一个变体瓦片 id（按 4…
