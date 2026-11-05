# je_tilemap_tileset_path

## 函数签名

```c
JE_API const char* je_tilemap_tileset_path(je_tilemap_document_t* map, int32_t idx);
```

## 描述

取地图文档引用的指定下标图集的路径，下标越界返回空串

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `idx` | `int32_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `const char*` | 见描述。 |
