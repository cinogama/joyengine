# je_tilemap_layer_shader

## 函数签名

```c
JE_API /* OPTIONAL: If layer has no shader style */ je_tilemap_shader_ref_t* je_tilemap_layer_shader(je_tilemap_document_t* map,
    je_layer_id_t layer);
```

## 描述

取图层样式的着色器实例，返回新建引用副本（用 je_tilemap_shader_ref_free 释放）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `layer` | `je_layer_id_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_tilemap_shader_ref_t*` | 见描述。 |

## 注意事项

- 未指定样式时返回 nullptr
