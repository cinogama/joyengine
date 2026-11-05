# je_tilemap_tile_property_get

## 函数签名

```c
JE_API bool je_tilemap_tile_property_get(je_TilesetDocument* tileset,
    je_TileId tile_id, const char* name, je_PropertyType type,
    /* OPTIONAL */ void* out_value);
```

## 描述

取指定 id 瓦片的属性默认值，数据经 out_value 写回（形态见 je_PropertyType）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_TilesetDocument*` | — |
| `tile_id` | `je_TileId` | — |
| `name` | `const char*` | — |
| `type` | `je_PropertyType` | — |
| `out_value` | `void*` | 可选。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 无值或类型与实际存储不符返回 false
