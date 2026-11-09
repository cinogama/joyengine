# je_tilemap_source_extra_pass_set

## 函数签名

```c
JE_API bool je_tilemap_source_extra_pass_set(je_tileset_document_t* tileset,
    je_source_id_t source_idx, int32_t extra_idx, int32_t pass);
```

## 描述

修改附加层级的绑定通道 id（>= 1，同源内通道唯一），渲染侧逐帧对齐自动重绑

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tileset` | `je_tileset_document_t*` | — |
| `source_idx` | `je_source_id_t` | — |
| `extra_idx` | `int32_t` | — |
| `pass` | `int32_t` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |
