# je_graphic_cmd_bind_texture_2d

## 函数签名

```c
JE_API void je_graphic_cmd_bind_texture_2d(
    je_gl_command_buffer_t* cmd, uint32_t pass, je_gl_texture_2d_t* texture);
```

## 描述

将指定类别的纹理绑定到 shader 的纹理声明（pass）上

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | — |
| `pass` | `uint32_t` | pass 取自 shader 反射（je_gl_shader_t::m_sampler_methods） |
| `texture` | `je_gl_texture_2d_t*` | — |

## 返回值

无返回值。

## 注意事项

- 必须在 pass 内调用
- pass 取自 shader 反射（je_gl_shader_t::m_sampler_methods）
- 同一 pass 重复绑定会覆盖此前的纹理

## 相关接口

- [je_graphic_cmd_bind_sampler](je_graphic_cmd_bind_sampler.md) - 为指定的采样器声明绑定独立采样器
