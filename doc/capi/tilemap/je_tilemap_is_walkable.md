# je_tilemap_is_walkable

## 函数签名

```c
JE_API int32_t je_tilemap_is_walkable(je_tilemap_document_t* map, int32_t x, int32_t y);
```

## 描述

取某格全图层合并的通行性（任一图层不可通行即不可通行），出界视为不可通行

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `x` | `int32_t` | — |
| `y` | `int32_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |

## 注意事项

- 返回 1/0
