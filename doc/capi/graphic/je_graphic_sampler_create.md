# je_graphic_sampler_create

## 函数签名

```c
JE_API je_gl_sampler_t* /* NOT NULL */ je_graphic_sampler_create(
    je_gl_context_t* context, const je_gl_sampler_desc_t* desc);
```

## 描述

根据描述创建一个独立采样器

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `context` | `je_gl_context_t*` | — |
| `desc` | `const je_gl_sampler_desc_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_sampler_t*` | 见描述。 |

## 注意事项

- 返回的采样器持有引用计数，需要使用 je_graphic_sampler_close 减少引用计数
- 未通过 je_graphic_cmd_bind_sampler 显式绑定时，使用 shader 声明的默认采样器

## 相关接口

- [je_graphic_sampler_close](je_graphic_sampler_close.md) - 关闭一个采样器（使得其引用计数减少）；如果资源被 `jegl_share_resource` 引用过多次，需要对应调用多次才能真正释放
- [je_graphic_cmd_bind_sampler](je_graphic_cmd_bind_sampler.md) - 为指定的采样器声明绑定独立采样器
