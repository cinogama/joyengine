# je_graphic_cmd_bind_texture_3d

## 函数签名

```c
JE_API void je_graphic_cmd_bind_texture_3d(
    jegl_command_buffer* cmd, uint32_t pass, jegl_texture_3d* texture);
```

## 描述

将指定类别的纹理绑定到 shader 的纹理声明（pass）上

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `jegl_command_buffer*` | — |
| `pass` | `uint32_t` | pass 取自 shader 反射（jegl_shader::m_sampler_methods） |
| `texture` | `jegl_texture_3d*` | — |

## 返回值

无返回值。

## 注意事项

- 必须在 pass 内调用
- pass 取自 shader 反射（jegl_shader::m_sampler_methods）
- 同一 pass 重复绑定会覆盖此前的纹理

## 相关接口

- [je_graphic_cmd_bind_sampler](je_graphic_cmd_bind_sampler.md) - 为指定的采样器声明绑定独立采样器
