# je_tilemap_tile_def_property_info

## 函数签名

```c
JE_API bool je_tilemap_tile_def_property_info(je_tileset_document_t* tileset,
    je_tile_id_t tile_id, int32_t index,
    /* OPTIONAL */ const char** name, /* OPTIONAL */ je_property_type_t* type);
```

## 描述

取某瓦片定义中指定序号的属性名称与类型（按名称升序）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `tile_id` | `je_tile_id_t` | — |
| `index` | `int32_t` | index 越界或该瓦片无定义返回 false |
| `name` | `const char**` | 可选。 |
| `type` | `je_property_type_t*` | 可选。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- index 越界或该瓦片无定义返回 false

## 相关接口

- [je_tilemap_tile_defs](je_tilemap_tile_defs.md) - 枚举图集中带有稀疏元数据定义的瓦片 id（按 id 升序），返回总数
