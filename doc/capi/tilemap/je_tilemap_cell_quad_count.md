# je_tilemap_cell_quad_count

## 函数签名

```c
JE_API int32_t je_tilemap_cell_quad_count(je_TilemapDocument* map,
    je_LayerId layer, int32_t x, int32_t y);
```

## 描述

取某格渲染子四边形的数量：普通瓦片/整块自动图块为 1，四象限自动图块为 4，空格为 0

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

## 相关接口

- [je_tilemap_cell_quad](je_tilemap_cell_quad.md) - 取某格第 idx 个渲染子四边形，经出参返回 (源纹理路径, UV 矩形 u…
