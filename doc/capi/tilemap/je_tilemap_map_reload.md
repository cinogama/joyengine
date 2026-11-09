# je_tilemap_map_reload

## 函数签名

```c
JE_API void je_tilemap_map_reload(const char* path);
```

## 描述

丢弃指定路径地图文档在缓存中的未保存修改，下次访问时从磁盘重读

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | — |

## 返回值

无返回值。
