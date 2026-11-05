# je_tilemap_tileset_version

## 函数签名

```c
JE_API uint64_t je_tilemap_tileset_version(je_TilesetDocument* tileset);
```

## 描述

取图集文档的修改版本号，任何经本组 API 的修改都会使其自增

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_TilesetDocument*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `uint64_t` | 见描述。 |
