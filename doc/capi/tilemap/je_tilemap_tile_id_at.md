# je_tilemap_tile_id_at

## 函数签名

```c
JE_API bool je_tilemap_tile_id_at(je_tileset_document_t* tileset,
    je_source_id_t source_idx, int32_t ix, int32_t iy, je_tile_id_t* out_id);
```

## 描述

取指定源纹理 (ix, iy) 格位的瓦片 id

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `source_idx` | `je_source_id_t` | — |
| `ix` | `int32_t` | — |
| `iy` | `int32_t` | — |
| `out_id` | `je_tile_id_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 源/格位越界返回 false；每个格恒有瓦片，无需注册
