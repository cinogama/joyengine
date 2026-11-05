# je_tilemap_tile_normal_set

## 函数签名

```c
JE_API bool je_tilemap_tile_normal_set(je_TilesetDocument* tileset,
    je_TileId tile_id, float x, float y, float z);
```

## 描述

设置指定 id 瓦片的顶点法线（按传入向量归一化后存储）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_TilesetDocument*` | — |
| `tile_id` | `je_TileId` | — |
| `x` | `float` | — |
| `y` | `float` | — |
| `z` | `float` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 地图网格生成时写入该瓦片的全部顶点；自动拼接格（四象限/变体）整格共享锚点瓦片的法线，取样出的变体瓦片不各取各的
- (0,0,0) 或含非有限分量的向量非法，返回 false 且不修改
- id 无效返回 false；设为默认 (0,0,-1) 即清除该瓦片的法线覆盖

## 相关接口

- [je_tilemap_tile_normal](je_tilemap_tile_normal.md) - 取指定 id 瓦片的顶点法线（局部空间单位向量），经出参返回 (x, y, …
