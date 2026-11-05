# je_tilemap_layer_z

## 函数签名

```c
JE_API float je_tilemap_layer_z(je_TilemapDocument* map, je_LayerId layer);
```

## 描述

取图层的世界 z 偏移（相对地图根实体；不同层须错开以避免深度冲突，旧格式文件缺省为层下标 0,1,2...）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `layer` | `je_LayerId` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `float` | 见描述。 |
