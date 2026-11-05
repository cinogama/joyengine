# je_tilemap_layer_grid

## 函数签名

```c
JE_API /* OPTIONAL: If layer is invalid */ const int32_t* je_tilemap_layer_grid(
    je_TilemapDocument* map, je_LayerId layer, /* OPTIONAL */ int32_t* out_count);
```

## 描述

取图层完整网格的只读视图（行优先，y 自上而下，长度 = width*height）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `layer` | `je_LayerId` | — |
| `out_count` | `int32_t*` | 可选。 out_count 返回元素数量（可为 nullptr） |

## 返回值

| 类型 | 描述 |
|------|------|
| `const int32_t*` | 见描述。 |

## 注意事项

- out_count 返回元素数量（可为 nullptr）
- 图层无效时返回 nullptr
- 指针在文档存活且网格未改动期间有效
- 供编辑器序列化等需要遍历整层的场景，避免逐格调用 get_tile
