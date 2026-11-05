# je_graphic_cmd_set_uniform_int3

## 函数签名

```c
JE_API void je_graphic_cmd_set_uniform_int3(
    jegl_command_buffer* cmd, jegl_uniform_slot slot, int x, int y, int z);
```

## 描述

以标量或整数向量的形式设置当前 pipeline 的 per-draw uniform

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `jegl_command_buffer*` | — |
| `slot` | `jegl_uniform_slot` | — |
| `x` | `int` | — |
| `y` | `int` | — |
| `z` | `int` | — |

## 返回值

无返回值。

## 注意事项

- 参数约定与 je_graphic_cmd_set_uniform 一致：必须先绑定 pipeline，slot 必须有效且大小匹配

## 相关接口

- [je_graphic_cmd_set_uniform](je_graphic_cmd_set_uniform.md) - 设置当前 pipeline 的 per-draw uniform。
