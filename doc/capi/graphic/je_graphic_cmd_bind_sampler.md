# je_graphic_cmd_bind_sampler

## 函数签名

```c
JE_API void je_graphic_cmd_bind_sampler(
    jegl_command_buffer* cmd, uint32_t sampler_id, jegl_sampler* sampler);
```

## 描述

为指定的采样器声明绑定独立采样器

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `jegl_command_buffer*` | — |
| `sampler_id` | `uint32_t` | sampler_id 取自 shader 反射（jegl_shader::m_sampler_methods） |
| `sampler` | `jegl_sampler*` | — |

## 返回值

无返回值。

## 注意事项

- 必须在 pass 内且已绑定 pipeline 后调用
- sampler_id 取自 shader 反射（jegl_shader::m_sampler_methods）
- 未绑定采样器的声明使用 shader 的默认采样器

## 相关接口

- [je_graphic_sampler_create](je_graphic_sampler_create.md) - 根据描述创建一个独立采样器
- [je_graphic_cmd_bind_texture_2d](je_graphic_cmd_bind_texture_2d.md) - 将指定类别的纹理绑定到 shader 的纹理声明（pass）上
