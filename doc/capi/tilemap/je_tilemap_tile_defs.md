# je_tilemap_tile_defs

## 函数签名

```c
JE_API int32_t je_tilemap_tile_defs(je_tileset_document_t* tileset,
    /* OPTIONAL */ je_tile_id_t* out_ids, int32_t capacity);
```

## 描述

枚举图集中带有稀疏元数据定义的瓦片 id（按 id 升序），返回总数

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `out_ids` | `je_tile_id_t*` | 可选。 out_ids 容量不足时只写入前 capacity 项 |
| `capacity` | `int32_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |

## 注意事项

- 未定义的瓦片按默认元数据（可通行、普通瓦片、无属性、空名称）处理
- out_ids 容量不足时只写入前 capacity 项
- out_ids 为 nullptr 或 capacity <= 0 时仅查询数量
- 供编辑器序列化 .je4tileset 的 [[tiles]] 段

## 相关接口

- [je_tilemap_tile_def_property_info](je_tilemap_tile_def_property_info.md) - 取某瓦片定义中指定序号的属性名称与类型（按名称升序）
