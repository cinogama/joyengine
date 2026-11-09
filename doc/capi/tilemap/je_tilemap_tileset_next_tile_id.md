# je_tilemap_tileset_next_tile_id

## 函数签名

```c
JE_API int32_t je_tilemap_tileset_next_tile_id(je_tileset_document_t* tileset);
```

## 描述

取图集文档下一个可分配的瓦片 id（只增不减：删除源/裁掉格子后其 id作废，不会被后续新增格复用）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |

## 注意事项

- 编辑器序列化 .je4tileset 时须原样保存，保证地图按 id 的引用不漂移
