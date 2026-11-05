# je_tilemap_layer_add

## 函数签名

```c
JE_API bool je_tilemap_layer_add(je_TilemapDocument* map, const char* name,
    je_LayerId* out_id);
```

## 描述

向地图文档末尾追加一个图层，新图层下标（je_LayerId）经 out_id 返回

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `name` | `const char*` | — |
| `out_id` | `je_LayerId*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 失败时不写出下标并返回 false
