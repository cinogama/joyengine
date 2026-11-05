# je_tilemap_layer_blockshadow

## 函数签名

```c
JE_API je_layer_block_shadow_t je_tilemap_layer_blockshadow(je_tilemap_document_t* map, je_layer_id_t layer);
```

## 描述

取图层的 2D 光照遮挡阴影行为（je_layer_block_shadow_t），下标越界时返回JE_LAYER_BLOCKSHADOW_OFF

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `layer` | `je_layer_id_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_layer_block_shadow_t` | 见描述。 |

## 相关接口

- [je_tilemap_layer_blockshadow_set](je_tilemap_layer_blockshadow_set.md) - 设置图层的 2D 光照遮挡阴影行为：OFF 时该层渲染分片移除 Light2…
