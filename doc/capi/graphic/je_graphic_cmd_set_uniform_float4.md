# je_graphic_cmd_set_uniform_float4

## 函数签名

```c
JE_API void je_graphic_cmd_set_uniform_float4(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, float x, float y, float z, float w);
```

## 描述

以标量或浮点向量的形式设置当前 pipeline 的 per-draw uniform

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | — |
| `slot` | `je_gl_uniform_slot_t` | — |
| `x` | `float` | — |
| `y` | `float` | — |
| `z` | `float` | — |
| `w` | `float` | — |

## 返回值

无返回值。

## 注意事项

- 参数约定与 je_graphic_cmd_set_uniform 一致：必须先绑定 pipeline，slot 必须有效且大小匹配

## 相关接口

- [je_graphic_cmd_set_uniform](je_graphic_cmd_set_uniform.md) - 设置当前 pipeline 的 per-draw uniform。
