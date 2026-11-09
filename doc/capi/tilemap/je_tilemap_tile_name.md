# je_tilemap_tile_name

## 函数签名

```c
JE_API const char* je_tilemap_tile_name(je_tileset_document_t* tileset, je_tile_id_t tile_id);
```

## 描述

取指定 id 瓦片的名称；未命名或 id 无效返回空串（指向文档内部存储，文档下次修改前有效）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `tile_id` | `je_tile_id_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `const char*` | 见描述。 |

## 相关接口

- [je_tilemap_tile_name_set](je_tilemap_tile_name_set.md) - 设置指定 id 瓦片的名称
