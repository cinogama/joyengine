# je_tilemap_map_version

## 函数签名

```c
JE_API uint64_t je_tilemap_map_version(je_tilemap_document_t* map);
```

## 描述

取地图文档的修改版本号，任何经本组 API 的修改都会使其自增

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `uint64_t` | 见描述。 |
