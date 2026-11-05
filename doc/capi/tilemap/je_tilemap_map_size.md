# je_tilemap_map_size

## 函数签名

```c
JE_API void je_tilemap_map_size(je_TilemapDocument* map,
    /* OPTIONAL */ int32_t* w, /* OPTIONAL */ int32_t* h, /* OPTIONAL */ int32_t* tile_px, /* OPTIONAL */ int32_t* layer_count);
```

## 描述

取地图文档的尺寸信息，经出参返回 (宽, 高, 瓦片边长像素, 图层数)

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `w` | `int32_t*` | 可选。 |
| `h` | `int32_t*` | 可选。 |
| `tile_px` | `int32_t*` | 可选。 |
| `layer_count` | `int32_t*` | 可选。 |

## 返回值

无返回值。

## 注意事项

- 各出参均可传 nullptr 跳过
