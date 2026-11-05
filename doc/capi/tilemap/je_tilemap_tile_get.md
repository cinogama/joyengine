# je_tilemap_tile_get

## 函数签名

```c
JE_API int32_t je_tilemap_tile_get(je_TilemapDocument* map, je_LayerId layer,
    int32_t x, int32_t y);
```

## 描述

取指定图层某格的瓦片值（编码见本段说明），越界返回 0（空格值亦为 0）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `layer` | `je_LayerId` | — |
| `x` | `int32_t` | — |
| `y` | `int32_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |
