# je_tilemap_property_info

## 函数签名

```c
JE_API bool je_tilemap_property_info(je_tileset_document_t* tileset, int32_t index,
    /* OPTIONAL */ const char** name, /* OPTIONAL */ je_property_type_t* type);
```

## 描述

取指定下标属性定义的信息，经出参返回 (名称, 类型)

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `index` | `int32_t` | — |
| `name` | `const char**` | 可选。 |
| `type` | `je_property_type_t*` | 可选。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 下标越界返回 false
