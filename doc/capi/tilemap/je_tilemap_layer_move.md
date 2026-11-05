# je_tilemap_layer_move

## 函数签名

```c
JE_API bool je_tilemap_layer_move(je_TilemapDocument* map, je_LayerId layer, je_LayerId new_pos);
```

## 描述

把图层移动到新下标（自动钳制到 [0, 图层数-1]），并重排逐格属性键

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `layer` | `je_LayerId` | — |
| `new_pos` | `je_LayerId` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |
