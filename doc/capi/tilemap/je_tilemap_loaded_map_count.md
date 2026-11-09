# je_tilemap_loaded_map_count

## 函数签名

```c
JE_API int32_t je_tilemap_loaded_map_count(void);
```

## 描述

返回进程内已注册的地图文档数量（含未保存的新建文档）

## 参数

无参数。

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |

## 注意事项

- 文档实例销毁后其枚举槽位置空，计数不变但 je_tilemap_loaded_map在该下标返回 nullptr；计数也含仍被引用、但已被 je_tilemap_map_reload弃用的旧实例（其路径已清空、不再被任何世界使用）

## 相关接口

- [je_tilemap_loaded_map](je_tilemap_loaded_map.md) - 按枚举下标取已注册地图文档的实例指针，与 je_tilemap_loaded…
