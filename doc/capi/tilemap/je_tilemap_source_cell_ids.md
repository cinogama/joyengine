# je_tilemap_source_cell_ids

## 函数签名

```c
JE_API /* OPTIONAL: If source_idx is invalid */ const int32_t*
    je_tilemap_source_cell_ids(je_TilesetDocument* tileset,
        je_SourceId source_idx, /* OPTIONAL */ int32_t* out_count);
```

## 描述

取指定源纹理逐格瓦片 id 表的只读视图（行优先，长度 = xcount*ycount）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_TilesetDocument*` | — |
| `source_idx` | `je_SourceId` | — |
| `out_count` | `int32_t*` | 可选。 out_count 返回元素数量（可为 nullptr） |

## 返回值

| 类型 | 描述 |
|------|------|
| `const int32_t*` | 见描述。 |

## 注意事项

- out_count 返回元素数量（可为 nullptr）
- 下标越界返回 nullptr；指针在文档存活且源网格未改动期间有效
- 供编辑器序列化原样保存（纹理尺寸变化后 id 与格坐标不是固定公式）

## 相关接口

- [je_tilemap_tile_id_at](je_tilemap_tile_id_at.md) - 取指定源纹理 (ix, iy) 格位的瓦片 id
