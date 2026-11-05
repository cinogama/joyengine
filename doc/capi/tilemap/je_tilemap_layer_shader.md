# je_tilemap_layer_shader

## 函数签名

```c
JE_API /* OPTIONAL: If layer has no shader style */ je_tilemap_shader_ref* je_tilemap_layer_shader(je_TilemapDocument* map,
    je_LayerId layer);
```

## 描述

取图层样式的着色器实例，返回新建引用副本（用 je_tilemap_shader_ref_free 释放）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `layer` | `je_LayerId` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_tilemap_shader_ref*` | 见描述。 |

## 注意事项

- 未指定样式时返回 nullptr
