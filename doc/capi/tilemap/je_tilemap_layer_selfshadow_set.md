# je_tilemap_layer_selfshadow_set

## 函数签名

```c
JE_API void je_tilemap_layer_selfshadow_set(je_tilemap_document_t* map, je_layer_id_t layer, je_layer_self_shadow_t selfshadow);
```

## 描述

设置图层的 2D 光照自阴影行为：OFF 时该层渲染分片移除 Light2D::SelfShadow组件（不参与自阴影覆盖），ON/AUTO 时挂载该组件并分别固定 auto_disable 为false / true；覆盖强度由 je_tilemap_layer_selfshadow_factor_set 设置（缺省 1）。非 TopDown 光源下自阴影为接收面语义：按 painter key 清除本层BlockShadow 落在本层图块上的自遮挡，同时抹掉位于该层之后的遮挡阴影，只保留位于该层之前的阴影；TopDown 光源下按逐块 key 并 MAX 合并（只加深阴影，不清除遮挡阴影）。渲染分片由引擎生成，组件的挂载与摘除由 TilemapSystem 逐帧对齐（走命令缓冲，下一帧迁移生效）；该设置随图层记录落盘

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `layer` | `je_layer_id_t` | — |
| `selfshadow` | `je_layer_self_shadow_t` | — |

## 返回值

无返回值。

## 注意事项

- 下标越界时不做任何修改

## 相关接口

- [je_tilemap_layer_selfshadow](je_tilemap_layer_selfshadow.md) - 取图层的 2D 光照自阴影行为（je_layer_self_shadow_t），下标…
- [je_tilemap_layer_selfshadow_factor_set](je_tilemap_layer_selfshadow_factor_set.md) - 设置图层的自阴影覆盖强度：不做范围钳制（与 Light2D::SelfSha…
