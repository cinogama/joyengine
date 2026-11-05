# je_tilemap_source_add

## 函数签名

```c
JE_API bool je_tilemap_source_add(je_TilesetDocument* tileset,
    const char* texture_path, int32_t tile_px, je_SourceId* out_id);
```

## 描述

向图集文档添加源纹理，tile_px 为瓦片边长（像素），网格数由纹理尺寸推得，源下标（je_SourceId）经 out_id 返回

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_TilesetDocument*` | — |
| `texture_path` | `const char*` | — |
| `tile_px` | `int32_t` | — |
| `out_id` | `je_SourceId*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 源的每个格都会分配稳定的瓦片 id（见 je_TileId），无需再注册瓦片
- 图集重新打开时网格按纹理当前尺寸重整（纹理增删一行/一列即增删一行/一列瓦片，已有瓦片 id 不变），见 je_tilemap_source_info
- 已有同路径源时返回 true 且写出其下标；失败（纹理无法加载或参数不合法）返回 false，不写出下标
