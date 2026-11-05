# je_tilemap_source_extra_info

## 函数签名

```c
JE_API bool je_tilemap_source_extra_info(je_tileset_document_t* tileset,
    je_source_id_t source_idx, int32_t extra_idx,
    /* OPTIONAL */ int32_t* pass, /* OPTIONAL */ const char** texture_path);
```

## 描述

取指定附加层级的信息，经出参返回 (绑定通道 id, 纹理路径)

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `source_idx` | `je_source_id_t` | — |
| `extra_idx` | `int32_t` | — |
| `pass` | `int32_t*` | 可选。 |
| `texture_path` | `const char**` | 可选。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 下标越界返回 false
