# je_tilemap_layer_add

## 函数签名

```c
JE_API bool je_tilemap_layer_add(je_tilemap_document_t* map, const char* name,
    je_layer_id_t* out_id);
```

## 描述

向地图文档末尾追加一个图层，新图层下标（je_layer_id_t）经 out_id 返回

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `name` | `const char*` | — |
| `out_id` | `je_layer_id_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 失败时不写出下标并返回 false
