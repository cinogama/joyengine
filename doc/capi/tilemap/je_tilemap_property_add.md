# je_tilemap_property_add

## 函数签名

```c
JE_API bool je_tilemap_property_add(je_tileset_document_t* tileset,
    const char* name, je_property_type_t type,
    /* OPTIONAL */ const void* default_value, int32_t* out_index);
```

## 描述

向图集 schema 添加属性定义（数据形态见 je_property_type_t），属性下标经out_index 返回

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `name` | `const char*` | — |
| `type` | `je_property_type_t` | — |
| `default_value` | `const void*` | 可选。 default_value 传 nullptr 时按类型取零值（false/""/0/0.0/全 0 向量） |
| `out_index` | `int32_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 重名、类型非法返回 false，不写出下标
- default_value 传 nullptr 时按类型取零值（false/""/0/0.0/全 0 向量）
