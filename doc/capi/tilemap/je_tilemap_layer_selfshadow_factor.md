# je_tilemap_layer_selfshadow_factor

## 函数签名

```c
JE_API float je_tilemap_layer_selfshadow_factor(je_TilemapDocument* map, je_LayerId layer);
```

## 描述

取图层的自阴影覆盖强度（写入分片 Light2D::SelfShadow::factor，即覆盖色RGBA 乘子；仅图层模式为 ON/AUTO 时生效），下标越界时返回 1

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `layer` | `je_LayerId` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `float` | 见描述。 |

## 相关接口

- [je_tilemap_layer_selfshadow_factor_set](je_tilemap_layer_selfshadow_factor_set.md) - 设置图层的自阴影覆盖强度：不做范围钳制（与 Light2D::SelfSha…
