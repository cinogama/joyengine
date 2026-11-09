# je_tilemap_tile_name_set

## 函数签名

```c
JE_API bool je_tilemap_tile_name_set(je_tileset_document_t* tileset,
    je_tile_id_t tile_id, const char* name);
```

## 描述

设置指定 id 瓦片的名称

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `tile_id` | `je_tile_id_t` | — |
| `name` | `const char*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- id 无效返回 false

## 相关接口

- [je_tilemap_tile_name](je_tilemap_tile_name.md) - 取指定 id 瓦片的名称；未命名或 id 无效返回空串（指向文档内部存储，文…
