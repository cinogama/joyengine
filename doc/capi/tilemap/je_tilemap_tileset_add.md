# je_tilemap_tileset_add

## 函数签名

```c
JE_API bool je_tilemap_tileset_add(je_tilemap_document_t* map, const char* path,
    int32_t* out_index);
```

## 描述

向地图文档登记一个图集引用，图集下标经 out_index 返回

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `path` | `const char*` | — |
| `out_index` | `int32_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 已引用同一路径时返回 true 且写出其下标；失败（路径为空）返回 false，不写出下标
- 不要求图集此刻可解析，可先引用后保存图集
