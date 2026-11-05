# je_tilemap_tile_normal

## 函数签名

```c
JE_API bool je_tilemap_tile_normal(je_tileset_document_t* tileset, je_tile_id_t tile_id,
    /* OPTIONAL */ float* out_x, /* OPTIONAL */ float* out_y, /* OPTIONAL */ float* out_z);
```

## 描述

取指定 id 瓦片的顶点法线（局部空间单位向量），经出参返回 (x, y, z)

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `tile_id` | `je_tile_id_t` | — |
| `out_x` | `float*` | 可选。 |
| `out_y` | `float*` | 可选。 |
| `out_z` | `float*` | 可选。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 默认 (0, 0, -1)，在引擎内指向屏幕外侧（朝向摄像机）
- 出参均可传 nullptr 跳过；id 无效返回 false 且不写出参

## 相关接口

- [je_tilemap_tile_normal_set](je_tilemap_tile_normal_set.md) - 设置指定 id 瓦片的顶点法线（按传入向量归一化后存储）
