# je_tilemap_map_resize

## 函数签名

```c
JE_API void je_tilemap_map_resize(je_TilemapDocument* map, int32_t new_w, int32_t new_h);
```

## 描述

调整地图文档的网格尺寸，越界格子的逐格属性覆盖将被丢弃

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `new_w` | `int32_t` | — |
| `new_h` | `int32_t` | — |

## 返回值

无返回值。

## 注意事项

- 新尺寸不合法（<=0 或超过 65536）时不做任何修改
