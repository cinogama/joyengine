# je_tilemap_cell_find_get

## 函数签名

```c
JE_API bool je_tilemap_cell_find_get(je_TilemapDocument* map, int32_t index, int32_t* x, int32_t* y);
```

## 描述

取出 je_tilemap_cell_find 结果中第 index 个格子，经出参返回其 (x, y)

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `index` | `int32_t` | — |
| `x` | `int32_t*` | — |
| `y` | `int32_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 越界返回 false

## 相关接口

- [je_tilemap_cell_find](je_tilemap_cell_find.md) - 按属性值查找格子，返回命中数量；结果按文档缓存，经 je_tilemap_c…
