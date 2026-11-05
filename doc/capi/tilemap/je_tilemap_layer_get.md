# je_tilemap_layer_get

## 函数签名

```c
JE_API void je_tilemap_layer_get(je_TilemapDocument* map, je_LayerId layer,
    /* OPTIONAL */ const char** name, /* OPTIONAL */ int32_t* visible, /* OPTIONAL */ int32_t* locked);
```

## 描述

取指定下标图层的信息，经出参返回 (名称, 是否可见, 是否锁定)

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `layer` | `je_LayerId` | — |
| `name` | `const char**` | 可选。 |
| `visible` | `int32_t*` | 可选。 |
| `locked` | `int32_t*` | 可选。 |

## 返回值

无返回值。

## 注意事项

- 各出参均可传 nullptr 跳过；下标越界时名称写出空串、其余写出 0
