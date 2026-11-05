# je_tilemap_map_unload

## 函数签名

```c
JE_API int32_t je_tilemap_map_unload(je_TilemapDocument* map);
```

## 描述

地图文档引用计数 -1（下限 0）；归零即销毁文档实例——指针立即失效，下次按路径打开自磁盘重读（未保存的修改随之丢弃）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_TilemapDocument*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |

## 注意事项

- 返回减少后的计数（0 表示文档已被销毁或不可再卸载）
- 线程安全：可从任意线程调用；Woolang 文档 gchandle 被并发 GC回收时即经终结器在 GC worker 线程上调用本函数

## 相关接口

- [je_tilemap_map_keep](je_tilemap_map_keep.md) - 地图文档引用计数 +1（文档一经加载默认长期驻留，引用计数是唯一的卸载途径）…
- [je_tilemap_map_open](je_tilemap_map_open.md) - 打开（或取进程内缓存）指定路径的地图文档，返回文档实例指针
