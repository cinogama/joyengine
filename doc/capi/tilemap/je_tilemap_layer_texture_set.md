# je_tilemap_layer_texture_set

## 函数签名

```c
JE_API bool je_tilemap_layer_texture_set(je_TilemapDocument* map, je_LayerId layer,
    int32_t slot, /* OPTIONAL */ je_tilemap_texture_ref* texture);
```

## 描述

设置（或替换/删除）图层样式某槽位的附加纹理

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `layer` | `je_LayerId` | — |
| `slot` | `int32_t` | — |
| `texture` | `je_tilemap_texture_ref*` | 可选。 texture 传 nullptr 表示删除该槽位的绑定 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 槽 0 保留给图集源纹理，slot < 1 时返回 false
- texture 传 nullptr 表示删除该槽位的绑定
- 同槽位重复设置时替换旧值
