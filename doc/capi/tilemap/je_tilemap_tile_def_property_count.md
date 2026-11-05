# je_tilemap_tile_def_property_count

## 函数签名

```c
JE_API int32_t je_tilemap_tile_def_property_count(je_tileset_document_t* tileset,
    je_tile_id_t tile_id);
```

## 描述

取某瓦片定义的默认属性数量（该瓦片无定义返回 0）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `tile_id` | `je_tile_id_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |
