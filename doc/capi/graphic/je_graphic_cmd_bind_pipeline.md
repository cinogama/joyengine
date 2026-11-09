# je_graphic_cmd_bind_pipeline

## 函数签名

```c
JE_API void je_graphic_cmd_bind_pipeline(
    je_gl_command_buffer_t* cmd, je_gl_pipeline_t* pipeline);
```

## 描述

为后续绘制绑定 pipeline

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | — |
| `pipeline` | `je_gl_pipeline_t*` | pipeline 必须与命令缓冲区属于同一个图形上下文 |

## 返回值

无返回值。

## 注意事项

- 必须在 pass 内调用
- pipeline 必须与命令缓冲区属于同一个图形上下文
- 绑定 pipeline 会按 shader 的默认值重新初始化 per-draw uniform 块

## 相关接口

- [je_graphic_pipeline_create](je_graphic_pipeline_create.md) - 创建一个 pipeline：shader + 固定功能状态 + 顶点布局兼容…
- [je_graphic_cmd_set_uniform](je_graphic_cmd_set_uniform.md) - 设置当前 pipeline 的 per-draw uniform。
