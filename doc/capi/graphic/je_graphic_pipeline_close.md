# je_graphic_pipeline_close

## 函数签名

```c
JE_API void je_graphic_pipeline_close(je_gl_pipeline_t* pipeline);
```

## 描述

关闭一个 pipeline 资源（使得其引用计数减少），如果资源被 jegl_share_resource引用过多次，那么需要对应调用多次才能真正释放资源

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `pipeline` | `je_gl_pipeline_t*` | — |

## 返回值

无返回值。

## 注意事项

- 若此操作使得引用计数变为 0，pipeline 持有的 shader 引用随之释放，对应的后端对象将由资源所属图形线程在适当的时机销毁

## 相关接口

- [je_graphic_pipeline_create](je_graphic_pipeline_create.md) - 创建一个 pipeline：shader + 固定功能状态 + 顶点布局兼容…
