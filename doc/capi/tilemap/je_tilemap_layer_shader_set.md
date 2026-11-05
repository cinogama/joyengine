# je_tilemap_layer_shader_set

## 函数签名

```c
JE_API void je_tilemap_layer_shader_set(je_TilemapDocument* map, je_LayerId layer,
    /* OPTIONAL */ je_tilemap_shader_ref* shader);
```

## 描述

设置图层样式的着色器实例

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `layer` | `je_LayerId` | — |
| `shader` | `je_tilemap_shader_ref*` | 可选。 shader 传 nullptr 表示清除样式（该层分片着色器列表被清空） |

## 返回值

无返回值。

## 注意事项

- shader 传 nullptr 表示清除样式（该层分片着色器列表被清空）
- 样式共享持有传入实例：uniform 值设在实例上，改动下一帧生效
- 下标越界时不做任何修改
