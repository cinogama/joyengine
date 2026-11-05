# je_tilemap_tile_info

## 函数签名

```c
JE_API bool je_tilemap_tile_info(je_TilesetDocument* tileset, je_TileId tile_id,
    /* OPTIONAL */ je_SourceId* source_idx, /* OPTIONAL */ int32_t* ix, /* OPTIONAL */ int32_t* iy, /* OPTIONAL */ int32_t* walkable);
```

## 描述

取指定 id 瓦片的信息，经出参返回 (源下标, 源内格位 ix/iy, 是否可通行)

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_TilesetDocument*` | — |
| `tile_id` | `je_TileId` | — |
| `source_idx` | `je_SourceId*` | 可选。 |
| `ix` | `int32_t*` | 可选。 |
| `iy` | `int32_t*` | 可选。 |
| `walkable` | `int32_t*` | 可选。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- id 不属于任何现存源格（越界/源已删除）返回 false
