# je_tilemap_loaded_map

## 函数签名

```c
JE_API /* OPTIONAL: If index is out of range or the slot is empty */ je_tilemap_document_t* je_tilemap_loaded_map(int32_t index);
```

## 描述

按枚举下标取已注册地图文档的实例指针，与 je_tilemap_loaded_map_count 配套遍历

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `index` | `int32_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_tilemap_document_t*` | 见描述。 |

## 注意事项

- 下标从 0 起、小于 je_tilemap_loaded_map_count；越界或该槽位实例已销毁返回 nullptr
- 枚举下标不是文档指针：文档指针稳定不变，而打开/新建文档会使后续枚举下标位移，遍历中不应再打开/新建文档

## 相关接口

- [je_tilemap_loaded_map_count](je_tilemap_loaded_map_count.md) - 返回进程内已注册的地图文档数量（含未保存的新建文档）
