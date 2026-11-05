# je_tilemap_layer_selfshadow_factor

## 函数签名

```c
JE_API float je_tilemap_layer_selfshadow_factor(je_tilemap_document_t* map, je_layer_id_t layer);
```

## 描述

取图层的自阴影覆盖强度（写入分片 Light2D::SelfShadow::factor，即覆盖色RGBA 乘子；仅图层模式为 ON/AUTO 时生效），下标越界时返回 1

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `layer` | `je_layer_id_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `float` | 见描述。 |

## 相关接口

- [je_tilemap_layer_selfshadow_factor_set](je_tilemap_layer_selfshadow_factor_set.md) - 设置图层的自阴影覆盖强度：不做范围钳制（与 Light2D::SelfSha…
