# je_tilemap_source_extra_add

## 函数签名

```c
JE_API bool je_tilemap_source_extra_add(je_TilesetDocument* tileset,
    je_SourceId source_idx, const char* texture_path, int32_t pass);
```

## 描述

向源纹理添加附加层级纹理，pass 为其绑定通道 id（>= 1；通道 0 由主纹理/反照率固定占用）。层级与主纹理共用同一瓦片网格（层号 = iy*xcount + ix对齐，cell 尺寸由层级自身分辨率推得），渲染时按 pass 绑定为独立数组纹理

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_TilesetDocument*` | — |
| `source_idx` | `je_SourceId` | — |
| `texture_path` | `const char*` | — |
| `pass` | `int32_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 同 (纹理路径, 通道) 幂等返回 true；同通道不同路径或同路径不同通道（换通道请用 je_tilemap_source_extra_pass_set）、纹理无法加载或尺寸与网格不符返回 false

## 相关接口

- [je_tilemap_source_info](je_tilemap_source_info.md) - 取指定下标源纹理的信息，经出参返回 (纹理路径, 瓦片边长, 横向格数, 纵…
