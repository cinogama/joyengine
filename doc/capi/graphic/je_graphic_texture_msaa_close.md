# je_graphic_texture_msaa_close

## 函数签名

```c
JE_API void je_graphic_texture_msaa_close(jegl_texture_msaa* texture);
```

## 描述

关闭一个对应类别的纹理资源（使得其引用计数减少），如果资源被 jegl_share_resource 引用过多次，那么需要对应调用多次才能真正释放资源；

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `texture` | `jegl_texture_msaa*` | — |

## 返回值

无返回值。

## 注意事项

- 如果此操作使得资源的引用计数变为 0，资源应当被立即视为不可用；对应的底层图形资源将由资源对应的图形线程上下文在适当的时机释放。
