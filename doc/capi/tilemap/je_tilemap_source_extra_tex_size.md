# je_tilemap_source_extra_tex_size

## 函数签名

```c
JE_API bool je_tilemap_source_extra_tex_size(je_tileset_document_t* tileset,
    je_source_id_t source_idx, int32_t extra_idx,
    /* OPTIONAL */ int32_t* tex_w, /* OPTIONAL */ int32_t* tex_h);
```

## 描述

取指定附加层级的实测像素尺寸，经出参返回 (tex_w, tex_h)

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `source_idx` | `je_source_id_t` | — |
| `extra_idx` | `int32_t` | — |
| `tex_w` | `int32_t*` | 可选。 |
| `tex_h` | `int32_t*` | 可选。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 尺寸未知（尚未测量）时写 0；下标越界返回 false
- 附加层级 cell 尺寸由该尺寸与主网格推得，编辑器序列化时原样保存
