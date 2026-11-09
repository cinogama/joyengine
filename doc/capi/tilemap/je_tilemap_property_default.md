# je_tilemap_property_default

## 函数签名

```c
JE_API bool je_tilemap_property_default(je_tileset_document_t* tileset,
    const char* name, je_property_type_t type, /* OPTIONAL */ void* out_value);
```

## 描述

按名称取图集 schema 中属性定义的默认值，数据经 out_value 写回（形态见 je_property_type_t）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `name` | `const char*` | — |
| `type` | `je_property_type_t` | — |
| `out_value` | `void*` | 可选。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 无此属性或类型与定义不符返回 false
