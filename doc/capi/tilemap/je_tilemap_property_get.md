# je_tilemap_property_get

## 函数签名

```c
JE_API bool je_tilemap_property_get(je_tilemap_document_t* map, je_layer_id_t layer,
    int32_t x, int32_t y, const char* name, je_property_type_t type,
    /* OPTIONAL */ void* out_value);
```

## 描述

按取值链（逐格覆盖 -> 瓦片默认值 -> 图集 schema 默认值）解析某格属性，数据经 out_value 写回（形态见 je_property_type_t）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `layer` | `je_layer_id_t` | — |
| `x` | `int32_t` | — |
| `y` | `int32_t` | — |
| `name` | `const char*` | — |
| `type` | `je_property_type_t` | — |
| `out_value` | `void*` | 可选。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 无值或类型与实际存储不符返回 false
