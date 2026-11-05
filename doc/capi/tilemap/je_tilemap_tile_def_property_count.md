# je_tilemap_tile_def_property_count

## 函数签名

```c
JE_API int32_t je_tilemap_tile_def_property_count(je_TilesetDocument* tileset,
    je_TileId tile_id);
```

## 描述

取某瓦片定义的默认属性数量（该瓦片无定义返回 0）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_TilesetDocument*` | — |
| `tile_id` | `je_TileId` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |
