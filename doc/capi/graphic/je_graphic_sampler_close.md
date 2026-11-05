# je_graphic_sampler_close

## 函数签名

```c
JE_API void je_graphic_sampler_close(jegl_sampler* sampler);
```

## 描述

关闭一个采样器（使得其引用计数减少），如果资源被 jegl_share_resource 引用过多次，那么需要对应调用多次才能真正释放资源

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `sampler` | `jegl_sampler*` | — |

## 返回值

无返回值。

## 注意事项

- 若此操作使得引用计数变为 0，对应的后端对象将由资源所属图形线程在适当的时机销毁

## 相关接口

- [je_graphic_sampler_create](je_graphic_sampler_create.md) - 根据描述创建一个独立采样器
