# je_tilemap_layer_rend_queue_set

## 函数签名

```c
JE_API void je_tilemap_layer_rend_queue_set(je_tilemap_document_t* map, je_layer_id_t layer, int32_t rend_queue);
```

## 描述

设置图层的渲染队列值

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `layer` | `je_layer_id_t` | — |
| `rend_queue` | `int32_t` | — |

## 返回值

无返回值。

## 相关接口

- [je_tilemap_layer_rend_queue](je_tilemap_layer_rend_queue.md) - 取图层的渲染队列值（Renderer::Rendqueue）：同图各层分片按…
