# je_tilemap_layer_blockshadow_factor_set

## 函数签名

```c
JE_API void je_tilemap_layer_blockshadow_factor_set(je_TilemapDocument* map, je_LayerId layer, float factor);
```

## 描述

设置图层的遮挡阴影浓度：不做范围钳制（与 Light2D::BlockShadow::factor组件本体同口径，常用 0..1）；浓度与模式一样随图层记录落盘、由TilemapSystem 逐帧对齐到分片组件

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `layer` | `je_LayerId` | — |
| `factor` | `float` | — |

## 返回值

无返回值。

## 注意事项

- 下标越界时不做任何修改

## 相关接口

- [je_tilemap_layer_blockshadow_factor](je_tilemap_layer_blockshadow_factor.md) - 取图层的遮挡阴影浓度（写入分片 Light2D::BlockShadow::…
- [je_tilemap_layer_blockshadow_set](je_tilemap_layer_blockshadow_set.md) - 设置图层的 2D 光照遮挡阴影行为：OFF 时该层渲染分片移除 Light2…
