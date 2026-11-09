# je_tilemap_tileset_remove

## 函数签名

```c
JE_API bool je_tilemap_tileset_remove(je_tilemap_document_t* map, int32_t idx);
```

## 描述

移除地图文档对指定下标图集的引用，并清除引用该图集的所有格子

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `idx` | `int32_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |
