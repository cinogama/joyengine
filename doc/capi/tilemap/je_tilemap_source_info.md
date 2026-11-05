# je_tilemap_source_info

## 函数签名

```c
JE_API bool je_tilemap_source_info(je_TilesetDocument* tileset, je_SourceId source_idx,
    /* OPTIONAL */ const char** texture_path, /* OPTIONAL */ int32_t* tile_px, /* OPTIONAL */ int32_t* xcount, /* OPTIONAL */ int32_t* ycount);
```

## 描述

取指定下标源纹理的信息，经出参返回 (纹理路径, 瓦片边长, 横向格数, 纵向格数)

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_TilesetDocument*` | — |
| `source_idx` | `je_SourceId` | — |
| `texture_path` | `const char**` | 可选。 |
| `tile_px` | `int32_t*` | 可选。 |
| `xcount` | `int32_t*` | 可选。 |
| `ycount` | `int32_t*` | 可选。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 行列数由纹理当前像素尺寸 / 瓦片边长推得（图集打开时重新测量），纹理增删一行/一列后随之增删
- 下标越界返回 false
