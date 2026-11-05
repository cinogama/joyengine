# je_tilemap_source_extra_count

## 函数签名

```c
JE_API int32_t je_tilemap_source_extra_count(je_tileset_document_t* tileset,
    je_source_id_t source_idx);
```

## 描述

取指定源纹理的附加层级纹理（如法线图集）数量

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `source_idx` | `je_source_id_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |

## 注意事项

- 下标越界返回 0
