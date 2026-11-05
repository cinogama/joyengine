# je_tilemap_layer_blockshadow

## 函数签名

```c
JE_API je_LayerBlockShadow je_tilemap_layer_blockshadow(je_TilemapDocument* map, je_LayerId layer);
```

## 描述

取图层的 2D 光照遮挡阴影行为（je_LayerBlockShadow），下标越界时返回JE_LAYER_BLOCKSHADOW_OFF

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `layer` | `je_LayerId` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_LayerBlockShadow` | 见描述。 |

## 相关接口

- [je_tilemap_layer_blockshadow_set](je_tilemap_layer_blockshadow_set.md) - 设置图层的 2D 光照遮挡阴影行为：OFF 时该层渲染分片移除 Light2…
