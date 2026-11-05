# je_tilemap_source_tex_size

## 函数签名

```c
JE_API bool je_tilemap_source_tex_size(je_TilesetDocument* tileset,
    je_SourceId source_idx,
    /* OPTIONAL */ int32_t* tex_w, /* OPTIONAL */ int32_t* tex_h);
```

## 描述

取指定源纹理的实测像素尺寸，经出参返回 (tex_w, tex_h)

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_TilesetDocument*` | — |
| `source_idx` | `je_SourceId` | — |
| `tex_w` | `int32_t*` | 可选。 |
| `tex_h` | `int32_t*` | 可选。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 尺寸未知（尚未测量）时写 0；下标越界返回 false
- 编辑器序列化 .je4tileset 时原样保存（加载时仍会按纹理重测）
