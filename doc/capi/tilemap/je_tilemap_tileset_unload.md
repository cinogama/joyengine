# je_tilemap_tileset_unload

## 函数签名

```c
JE_API int32_t je_tilemap_tileset_unload(je_tileset_document_t* tileset);
```

## 描述

图集文档引用计数 -1（下限 0）；归零即撤销注册表对该实例的持有——已无引用方（引用它的地图等）时实例销毁、指针立即失效，下次按路径打开（或地图解析）自磁盘重读（未保存的修改随之丢弃）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |

## 注意事项

- 返回减少后的计数（0 表示文档已被销毁或不可再卸载）

## 相关接口

- [je_tilemap_tileset_keep](je_tilemap_tileset_keep.md) - 图集文档引用计数 +1，返回增加后的计数
