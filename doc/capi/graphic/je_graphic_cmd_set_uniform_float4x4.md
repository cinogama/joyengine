# je_graphic_cmd_set_uniform_float4x4

## 函数签名

```c
JE_API void je_graphic_cmd_set_uniform_float4x4(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot, const float (*mat)[4]);
```

## 描述

以矩阵的形式设置当前 pipeline 的 per-draw uniform

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | — |
| `slot` | `je_gl_uniform_slot_t` | — |
| `mat` | `const float (*)[4]` | — |

## 返回值

无返回值。

## 注意事项

- 矩阵按连续内存写入 uniform 块；float3x3 的每个 3 分量向量按 std140规则占用 16 字节
- 参数约定与 je_graphic_cmd_set_uniform 一致：必须先绑定 pipeline，slot 必须有效且大小匹配

## 相关接口

- [je_graphic_cmd_set_uniform](je_graphic_cmd_set_uniform.md) - 设置当前 pipeline 的 per-draw uniform。
