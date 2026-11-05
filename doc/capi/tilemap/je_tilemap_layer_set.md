# je_tilemap_layer_set

## 函数签名

```c
JE_API void je_tilemap_layer_set(je_tilemap_document_t* map, je_layer_id_t layer,
    /* OPTIONAL */ const char* name, int32_t visible, int32_t locked);
```

## 描述

修改指定下标图层的名称、可见性与锁定状态

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `map` | `je_tilemap_document_t*` | — |
| `layer` | `je_layer_id_t` | — |
| `name` | `const char*` | 可选。 name 传 nullptr 表示保持名称不变 |
| `visible` | `int32_t` | — |
| `locked` | `int32_t` | — |

## 返回值

无返回值。

## 注意事项

- name 传 nullptr 表示保持名称不变
- 下标越界时不做任何修改
