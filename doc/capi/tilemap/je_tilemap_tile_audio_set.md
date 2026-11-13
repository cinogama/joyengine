# je_tilemap_tile_audio_set

## 函数签名

```c
JE_API bool je_tilemap_tile_audio_set(je_tileset_document_t* tileset,
    je_tile_id_t tile_id, float occlusion, float muffle);
```

## 描述

设置指定 id 瓦片的音频遮挡/闷音系数。两个系数都会夹取到 `[0, 1]`；含非有限分量（NaN/Inf）返回 `false` 且不修改。

默认 `(0, 0)` = 不遮挡/不闷音。两者都回到 0 且该瓦片没有其它元数据（名称、通行性覆盖、法线覆盖、自动拼接、变体、属性）时，稀疏元数据条目会被清理。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | 图集文档 |
| `tile_id` | `je_tile_id_t` | 瓦片 id |
| `occlusion` | `float` | 遮挡系数，夹取到 `[0, 1]` |
| `muffle` | `float` | 闷音系数，夹取到 `[0, 1]` |

## 返回值

`tile_id` 有效且系数有限返回 `true`，否则返回 `false`。

## 用法示例

```cpp
// 一堵完全隔音且严重闷音的墙
je_tilemap_tile_audio_set(tileset, wall_tile_id, 1.0f, 0.8f);

// 清除音频系数
je_tilemap_tile_audio_set(tileset, wall_tile_id, 0.0f, 0.0f);
```

## 注意事项

- 系数随图集缓存（`TILESET_CACHE_VERSION` v2 起）保存；
- 音频系统按地图/图集版本缓存平整化的遮挡网格，修改后下一次音频更新会自动重建。

## 相关接口

- [je_tilemap_tile_audio](je_tilemap_tile_audio.md) - 读取音频遮挡/闷音系数
