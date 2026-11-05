# je_tilemap_cell_find

## 函数签名

```c
JE_API int32_t je_tilemap_cell_find(je_TilemapDocument* map, je_LayerId layer,
    const char* name, je_PropertyType type, const void* value);
```

## 描述

按属性值查找格子，返回命中数量；结果按文档缓存，经 je_tilemap_cell_find_get 逐个取出

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |
| `layer` | `je_LayerId` | layer < 0 时搜索全部图层 |
| `name` | `const char*` | name 为 "walkable" 时按通行性匹配，type 须为 BOOL，value 指向 bool |
| `type` | `je_PropertyType` | — |
| `value` | `const void*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |

## 注意事项

- layer < 0 时搜索全部图层
- name 为 "walkable" 时按通行性匹配，type 须为 BOOL，value 指向 bool

## 相关接口

- [je_tilemap_cell_find_get](je_tilemap_cell_find_get.md) - 取出 je_tilemap_cell_find 结果中第 index 个格子…
