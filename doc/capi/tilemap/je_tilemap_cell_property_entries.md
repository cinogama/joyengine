# je_tilemap_cell_property_entries

## 函数签名

```c
JE_API int32_t je_tilemap_cell_property_entries(je_TilemapDocument* map,
    /* OPTIONAL */ je_TilemapCellPropertyEntry* out_entries, int32_t capacity);
```

## 描述

枚举文档中全部逐格属性覆盖条目（按 (layer, cell) 升序），返回条目总数

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `out_entries` | `je_TilemapCellPropertyEntry*` | 可选。 out_entries 容量不足时只写入前 capacity 项 |
| `capacity` | `int32_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |

## 注意事项

- out_entries 容量不足时只写入前 capacity 项
- out_entries 为 nullptr 或 capacity <= 0 时仅查询数量

## 相关接口

- [je_tilemap_cell_property_entry_property_info](je_tilemap_cell_property_entry_property_info.md) - 取某格覆盖表中指定序号的属性名称与类型
