# je_tilemap_source_extra_remove

## 函数签名

```c
JE_API bool je_tilemap_source_extra_remove(je_tileset_document_t* tileset,
    je_source_id_t source_idx, int32_t extra_idx);
```

## 描述

移除源纹理的指定附加层级（按层级下标），其余层级下标前移

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `source_idx` | `je_source_id_t` | — |
| `extra_idx` | `int32_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |
