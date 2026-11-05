# je_tilemap_tile_count

## 函数签名

```c
JE_API int32_t je_tilemap_tile_count(je_tileset_document_t* tileset);
```

## 描述

取图集文档中的瓦片总数（各源纹理的格子数之和；每个格都是一块瓦片）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |

## 相关接口

- [je_tilemap_tile_at_index](je_tilemap_tile_at_index.md) - 按枚举下标取瓦片 id（按源的添加顺序、源内行优先展开）
