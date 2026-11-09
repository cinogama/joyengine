# je_tilemap_tile_kind

## 函数签名

```c
JE_API je_tile_kind_t je_tilemap_tile_kind(je_tileset_document_t* tileset, je_tile_id_t tile_id);
```

## 描述

取指定 id 瓦片的自动拼接类型；id 无效按 JE_TILE_KIND_PLAIN 返回

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `tile_id` | `je_tile_id_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_tile_kind_t` | 见描述。 |
