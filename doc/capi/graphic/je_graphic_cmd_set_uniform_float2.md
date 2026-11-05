# je_graphic_cmd_set_uniform_float2

## 函数签名

```c
JE_API void je_graphic_cmd_set_uniform_float2(
    jegl_command_buffer* cmd, jegl_uniform_slot slot, float x, float y);
```

## 描述

以标量或浮点向量的形式设置当前 pipeline 的 per-draw uniform

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `jegl_command_buffer*` | — |
| `slot` | `jegl_uniform_slot` | — |
| `x` | `float` | — |
| `y` | `float` | — |

## 返回值

无返回值。

## 注意事项

- 参数约定与 je_graphic_cmd_set_uniform 一致：必须先绑定 pipeline，slot 必须有效且大小匹配

## 相关接口

- [je_graphic_cmd_set_uniform](je_graphic_cmd_set_uniform.md) - 设置当前 pipeline 的 per-draw uniform。
