# je_tilemap_layer_remove

## 函数签名

```c
JE_API bool je_tilemap_layer_remove(je_TilemapDocument* map, je_LayerId layer);
```

## 描述

移除指定下标的图层，并重排逐格属性键

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `layer` | `je_LayerId` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 下标越界返回 false
