# je_tilemap_layer_texture_get

## 函数签名

```c
JE_API bool je_tilemap_layer_texture_get(je_tilemap_document_t* map, je_layer_id_t layer,
    int32_t index, /* OPTIONAL */ int32_t* slot, /* OPTIONAL */ je_tilemap_texture_ref_t** texture);
```

## 描述

按序号（非槽位）取图层样式的附加纹理，经出参返回 (槽位, 纹理实例)

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `layer` | `je_layer_id_t` | — |
| `index` | `int32_t` | — |
| `slot` | `int32_t*` | 可选。 |
| `texture` | `je_tilemap_texture_ref_t**` | 可选。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 纹理实例为新建引用副本（用 je_tilemap_texture_ref_free 释放）
- 各出参均可传 nullptr 跳过；序号越界返回 false
