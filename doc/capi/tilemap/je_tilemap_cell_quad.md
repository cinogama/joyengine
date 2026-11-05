# je_tilemap_cell_quad

## 函数签名

```c
JE_API bool je_tilemap_cell_quad(je_TilemapDocument* map, je_LayerId layer,
    int32_t x, int32_t y, int32_t idx,
    /* OPTIONAL */ const char** texture_path,
    /* OPTIONAL */ float* u0, /* OPTIONAL */ float* v0, /* OPTIONAL */ float* u1, /* OPTIONAL */ float* v1,
    /* OPTIONAL */ int32_t* qx, /* OPTIONAL */ int32_t* qy, /* OPTIONAL */ int32_t* qcols);
```

## 描述

取某格第 idx 个渲染子四边形，经出参返回 (源纹理路径, UV 矩形 u0/v0/u1/v1引擎空间, 目标象限 qx/qy 于 qcols×qcols 网格、自左上角起)

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `layer` | `je_LayerId` | — |
| `x` | `int32_t` | — |
| `y` | `int32_t` | — |
| `idx` | `int32_t` | idx 越界或格子为空返回 false |
| `texture_path` | `const char**` | 可选。 |
| `u0` | `float*` | 可选。 |
| `v0` | `float*` | 可选。 |
| `u1` | `float*` | 可选。 |
| `v1` | `float*` | 可选。 |
| `qx` | `int32_t*` | 可选。 |
| `qy` | `int32_t*` | 可选。 |
| `qcols` | `int32_t*` | 可选。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- idx 越界或格子为空返回 false

## 相关接口

- [je_tilemap_cell_quad_count](je_tilemap_cell_quad_count.md) - 取某格渲染子四边形的数量：普通瓦片/整块自动图块为 1，四象限自动图块为 4…
