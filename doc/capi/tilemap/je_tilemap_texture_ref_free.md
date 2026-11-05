# je_tilemap_texture_ref_free

## 函数签名

```c
JE_API void je_tilemap_texture_ref_free(/* OPTIONAL */ je_tilemap_texture_ref_t* texture);
```

## 描述

释放 je_tilemap_layer_texture_get 出参返回的纹理引用副本（传 nullptr 安全）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `texture` | `je_tilemap_texture_ref_t*` | 可选。 |

## 返回值

无返回值。
