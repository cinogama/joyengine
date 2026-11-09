# je_tilemap_layer_blockshadow_factor

## 函数签名

```c
JE_API float je_tilemap_layer_blockshadow_factor(je_tilemap_document_t* map, je_layer_id_t layer);
```

## 描述

取图层的遮挡阴影浓度（写入分片 Light2D::BlockShadow::factor，阴影体浓度；仅图层模式为 ON/AUTO 时生效），下标越界时返回 1

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

- [je_tilemap_layer_blockshadow_factor_set](je_tilemap_layer_blockshadow_factor_set.md) - 设置图层的遮挡阴影浓度：不做范围钳制（与 Light2D::BlockSha…
