# je_tilemap_tile_fill

## 函数签名

```c
JE_API void je_tilemap_tile_fill(je_TilemapDocument* map, je_LayerId layer,
    int32_t x, int32_t y, int32_t w, int32_t h, int32_t value);
```

## 描述

把矩形区域内的格子批量设为同一瓦片值，供矩形/填充工具使用

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `layer` | `je_LayerId` | — |
| `x` | `int32_t` | — |
| `y` | `int32_t` | — |
| `w` | `int32_t` | — |
| `h` | `int32_t` | — |
| `value` | `int32_t` | — |

## 返回值

无返回值。

## 注意事项

- 自动跳过越界格子；无实际变化时不自增版本号
