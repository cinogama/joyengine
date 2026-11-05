# je_tilemap_layer_rend_queue

## 函数签名

```c
JE_API int32_t je_tilemap_layer_rend_queue(je_tilemap_document_t* map, je_layer_id_t layer);
```

## 描述

取图层的渲染队列值（Renderer::Rendqueue）：同图各层分片按该值升序绘制（画家算法，层间叠放的权威顺序）。新建图层缺省为 10000 + 层下标（与场景实体常用的小队列值错开）；z 只在该层与写深度的场景物体之间参与遮挡裁决

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `layer` | `je_layer_id_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |
