# je_tilemap_cell_property_set

## 函数签名

```c
JE_API bool je_tilemap_cell_property_set(je_tilemap_document_t* map, je_layer_id_t layer,
    int32_t x, int32_t y, const char* name, je_property_type_t type,
    /* OPTIONAL */ const void* value);
```

## 描述

设置某格的属性逐格覆盖值（数据形态见 je_property_type_t）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `layer` | `je_layer_id_t` | — |
| `x` | `int32_t` | — |
| `y` | `int32_t` | — |
| `name` | `const char*` | — |
| `type` | `je_property_type_t` | — |
| `value` | `const void*` | 可选。 value 传 nullptr 表示清除该格此属性的覆盖（此时忽略 type） |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- value 传 nullptr 表示清除该格此属性的覆盖（此时忽略 type）
- 该格已有不同类型的覆盖值时返回 false 且不修改
- 坐标越界返回 false
