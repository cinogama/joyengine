# je_tilemap_tileset_keep

## 函数签名

```c
JE_API int32_t je_tilemap_tileset_keep(je_tileset_document_t* tileset);
```

## 描述

图集文档引用计数 +1，返回增加后的计数

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |

## 注意事项

- 引用来源：open/create 的调用方、引用该图集的各地图（各占 1）

## 相关接口

- [je_tilemap_tileset_unload](je_tilemap_tileset_unload.md) - 图集文档引用计数 -1（下限 0）；归零即撤销注册表对该实例的持有——已无引…
