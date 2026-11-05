# je_tilemap_tile_property_set

## 函数签名

```c
JE_API bool je_tilemap_tile_property_set(je_TilesetDocument* tileset, je_TileId tile_id,
    const char* name, je_PropertyType type, /* OPTIONAL */ const void* value);
```

## 描述

设置指定 id 瓦片的属性默认值（数据形态见 je_PropertyType）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_TilesetDocument*` | — |
| `tile_id` | `je_TileId` | — |
| `name` | `const char*` | — |
| `type` | `je_PropertyType` | — |
| `value` | `const void*` | 可选。 value 传 nullptr 表示清除该默认值（此时忽略 type） |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- value 传 nullptr 表示清除该默认值（此时忽略 type）
- 已有不同类型的默认值时返回 false 且不修改
