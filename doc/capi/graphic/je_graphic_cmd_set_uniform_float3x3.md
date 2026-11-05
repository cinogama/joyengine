# je_graphic_cmd_set_uniform_float3x3

## 函数签名

```c
JE_API void je_graphic_cmd_set_uniform_float3x3(
    jegl_command_buffer* cmd, jegl_uniform_slot slot, const float (*mat)[3]);
```

## 描述

以矩阵的形式设置当前 pipeline 的 per-draw uniform

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `jegl_command_buffer*` | — |
| `slot` | `jegl_uniform_slot` | — |
| `mat` | `const float (*)[3]` | — |

## 返回值

无返回值。

## 注意事项

- 矩阵按连续内存写入 uniform 块；float3x3 的每个 3 分量向量按 std140规则占用 16 字节
- 参数约定与 je_graphic_cmd_set_uniform 一致：必须先绑定 pipeline，slot 必须有效且大小匹配

## 相关接口

- [je_graphic_cmd_set_uniform](je_graphic_cmd_set_uniform.md) - 设置当前 pipeline 的 per-draw uniform。
