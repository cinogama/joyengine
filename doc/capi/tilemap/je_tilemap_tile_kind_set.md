# je_tilemap_tile_kind_set

## 函数签名

```c
JE_API bool je_tilemap_tile_kind_set(je_tileset_document_t* tileset,
    je_tile_id_t tile_id, je_tile_kind_t kind);
```

## 描述

设置指定 id 瓦片的自动拼接类型（kind 取值见 je_tile_kind_t）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `tile_id` | `je_tile_id_t` | — |
| `kind` | `je_tile_kind_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- id 无效或 kind 非法返回 false
- 地图格存负值时按该类型解析邻接变体；PLAIN 即普通整块瓦片
