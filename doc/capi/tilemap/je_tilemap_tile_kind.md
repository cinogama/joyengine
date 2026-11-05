# je_tilemap_tile_kind

## 函数签名

```c
JE_API je_TileKind je_tilemap_tile_kind(je_TilesetDocument* tileset, je_TileId tile_id);
```

## 描述

取指定 id 瓦片的自动拼接类型；id 无效按 JE_TILE_KIND_PLAIN 返回

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_TilesetDocument*` | — |
| `tile_id` | `je_TileId` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_TileKind` | 见描述。 |
