# je_tilemap_layer_z_set

## 函数签名

```c
JE_API void je_tilemap_layer_z_set(je_tilemap_document_t* map, je_layer_id_t layer, float z);
```

## 描述

设置图层的世界 z 偏移

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `layer` | `je_layer_id_t` | — |
| `z` | `float` | — |

## 返回值

无返回值。

## 相关接口

- [je_tilemap_layer_z](je_tilemap_layer_z.md) - 取图层的世界 z 偏移（相对地图根实体；不同层须错开以避免深度冲突，旧格式文…
