# je_tilemap_tile_property_set

## 函数签名

```c
JE_API bool je_tilemap_tile_property_set(je_tileset_document_t* tileset, je_tile_id_t tile_id,
    const char* name, je_property_type_t type, /* OPTIONAL */ const void* value);
```

## 描述

设置指定 id 瓦片的属性默认值（数据形态见 je_property_type_t）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `tile_id` | `je_tile_id_t` | — |
| `name` | `const char*` | — |
| `type` | `je_property_type_t` | — |
| `value` | `const void*` | 可选。 value 传 nullptr 表示清除该默认值（此时忽略 type） |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- value 传 nullptr 表示清除该默认值（此时忽略 type）
- 已有不同类型的默认值时返回 false 且不修改
