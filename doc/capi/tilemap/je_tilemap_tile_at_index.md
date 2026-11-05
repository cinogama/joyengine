# je_tilemap_tile_at_index

## 函数签名

```c
JE_API bool je_tilemap_tile_at_index(je_tileset_document_t* tileset, int32_t index,
    je_tile_id_t* out_id);
```

## 描述

按枚举下标取瓦片 id（按源的添加顺序、源内行优先展开）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `index` | `int32_t` | — |
| `out_id` | `je_tile_id_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 下标越界返回 false；配合 je_tilemap_tile_count 遍历全部瓦片

## 相关接口

- [je_tilemap_tile_count](je_tilemap_tile_count.md) - 取图集文档中的瓦片总数（各源纹理的格子数之和；每个格都是一块瓦片）
