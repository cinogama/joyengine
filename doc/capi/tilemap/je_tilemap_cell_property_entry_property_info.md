# je_tilemap_cell_property_entry_property_info

## 函数签名

```c
JE_API bool je_tilemap_cell_property_entry_property_info(
    je_tilemap_document_t* map, je_layer_id_t layer, int32_t x, int32_t y,
    int32_t index, /* OPTIONAL */ const char** name,
    /* OPTIONAL */ je_property_type_t* type);
```

## 描述

取某格覆盖表中指定序号的属性名称与类型

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `layer` | `je_layer_id_t` | — |
| `x` | `int32_t` | — |
| `y` | `int32_t` | — |
| `index` | `int32_t` | index 越界或该格无覆盖时返回 false |
| `name` | `const char**` | 可选。 |
| `type` | `je_property_type_t*` | 可选。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- index 越界或该格无覆盖时返回 false

## 相关接口

- [je_tilemap_cell_property_entries](je_tilemap_cell_property_entries.md) - 枚举文档中全部逐格属性覆盖条目（按 (layer, cell) 升序），返回…
