# je_tilemap_layer_move

## 函数签名

```c
JE_API bool je_tilemap_layer_move(je_tilemap_document_t* map, je_layer_id_t layer, je_layer_id_t new_pos);
```

## 描述

把图层移动到新下标（自动钳制到 [0, 图层数-1]），并重排逐格属性键

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `layer` | `je_layer_id_t` | — |
| `new_pos` | `je_layer_id_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |
