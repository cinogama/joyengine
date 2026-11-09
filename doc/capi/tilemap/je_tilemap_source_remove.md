# je_tilemap_source_remove

## 函数签名

```c
JE_API bool je_tilemap_source_remove(je_tileset_document_t* tileset, je_source_id_t source_idx);
```

## 描述

移除指定下标的源纹理：该源格子的瓦片元数据随之丢弃，其余源下标前移

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `source_idx` | `je_source_id_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |
