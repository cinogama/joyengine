# je_tilemap_tileset_create

## 函数签名

```c
JE_API je_TilesetDocument* je_tilemap_tileset_create(const char* name);
```

## 描述

新建一个未保存的图集文档，返回文档实例指针

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `name` | `const char*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_TilesetDocument*` | 见描述。 |

## 注意事项

- 调用方持有 1 个文档引用（见 je_tilemap_tileset_unload）
