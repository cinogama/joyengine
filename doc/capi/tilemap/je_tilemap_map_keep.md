# je_tilemap_map_keep

## 函数签名

```c
JE_API int32_t je_tilemap_map_keep(je_tilemap_document_t* map);
```

## 描述

地图文档引用计数 +1（文档一经加载默认长期驻留，引用计数是唯一的卸载途径），返回增加后的计数

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `int32_t` | 见描述。 |

## 注意事项

- 引用来源：open/create 的调用方、使用该地图的各世界（各占 1）
- 线程安全：可从任意线程（含 Woolang GC 终结器线程）调用

## 相关接口

- [je_tilemap_map_unload](je_tilemap_map_unload.md) - 地图文档引用计数 -1（下限 0）；归零即销毁文档实例——指针立即失效，下次…
