# je_tilemap_layer_z

## 函数签名

```c
JE_API float je_tilemap_layer_z(je_tilemap_document_t* map, je_layer_id_t layer);
```

## 描述

取图层的世界 z 偏移（相对地图根实体；不同层须错开以避免深度冲突，旧格式文件缺省为层下标 0,1,2...）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `layer` | `je_layer_id_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `float` | 见描述。 |
