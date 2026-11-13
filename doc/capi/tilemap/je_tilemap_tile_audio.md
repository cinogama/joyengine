# je_tilemap_tile_audio

## 函数签名

```c
JE_API bool je_tilemap_tile_audio(je_tileset_document_t* tileset, je_tile_id_t tile_id,
    /* OPTIONAL */ float* out_occlusion, /* OPTIONAL */ float* out_muffle);
```

## 描述

取指定 id 瓦片的音频遮挡系数，经出参返回 `(occlusion, muffle)`：

* `occlusion`：0 = 不遮挡，1 = 完全隔音；音频系统对声源射线穿过的格子按 `(1 - 值)` 累乘增益；
* `muffle`：0 = 不闷音，1 = 完全失去高频；同样按 `(1 - 值)` 累乘高频增益；
* 两者默认均为 0，没有元数据的普通瓦片不遮挡。

出参均可传 `nullptr` 跳过；`tile_id` 不属于任何现存源格（越界/源已删除）返回 `false` 且不写出参。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | 图集文档 |
| `tile_id` | `je_tile_id_t` | 瓦片 id |
| `out_occlusion` | `float*` | 可选出参，接收遮挡系数 |
| `out_muffle` | `float*` | 可选出参，接收闷音系数 |

## 返回值

`tile_id` 有效返回 `true`，否则返回 `false`。

## 用法示例

```cpp
float occlusion = 0.f, muffle = 0.f;
if (je_tilemap_tile_audio(tileset, tile_id, &occlusion, &muffle))
{
    // occlusion/muffle 均为 [0, 1]
}
```

## 相关接口

- [je_tilemap_tile_audio_set](je_tilemap_tile_audio_set.md) - 设置音频遮挡/闷音系数
- [je_tilemap_tile_info](je_tilemap_tile_info.md) - 取瓦片的源下标/格位/通行性
