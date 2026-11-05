# je_tilemap_layer_blockshadow_set

## 函数签名

```c
JE_API void je_tilemap_layer_blockshadow_set(je_tilemap_document_t* map, je_layer_id_t layer, je_layer_block_shadow_t blockshadow);
```

## 描述

设置图层的 2D 光照遮挡阴影行为：OFF 时该层渲染分片移除 Light2D::BlockShadow组件（瓦片不遮挡光线），ON/AUTO 时挂载该组件并分别固定 auto_disable 为false / true；阴影浓度由 je_tilemap_layer_blockshadow_factor_set 设置（缺省 1）。遮挡轮廓（该层有效瓦片的占据边界，贪心合并共线段）由TilemapSystem 自动生成并随网格编辑重建；该设置随图层记录落盘

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `layer` | `je_layer_id_t` | — |
| `blockshadow` | `je_layer_block_shadow_t` | — |

## 返回值

无返回值。

## 注意事项

- 下标越界时不做任何修改

## 相关接口

- [je_tilemap_layer_blockshadow](je_tilemap_layer_blockshadow.md) - 取图层的 2D 光照遮挡阴影行为（je_layer_block_shadow_t），…
- [je_tilemap_layer_blockshadow_factor_set](je_tilemap_layer_blockshadow_factor_set.md) - 设置图层的遮挡阴影浓度：不做范围钳制（与 Light2D::BlockSha…
