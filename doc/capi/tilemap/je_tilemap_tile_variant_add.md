# je_tilemap_tile_variant_add

## 函数签名

```c
JE_API bool je_tilemap_tile_variant_add(je_tileset_document_t* tileset,
    je_tile_id_t tile_id, je_tile_id_t variant_tile_id);
```

## 描述

向指定 id 瓦片的 blob47 显式变体表追加一个变体瓦片 id（按 47 变体序）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `tile_id` | `je_tile_id_t` | — |
| `variant_tile_id` | `je_tile_id_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 瓦片 id 或变体 id 无效返回 false

## 相关接口

- [je_tilemap_tile_variant_clear](je_tilemap_tile_variant_clear.md) - 清空指定 id 瓦片的 blob47 显式变体表；空表时按锚点行优先扫描 4…
