# je_graphic_cmd_bind_vertex

## 函数签名

```c
JE_API void je_graphic_cmd_bind_vertex(
    je_gl_command_buffer_t* cmd, je_gl_vertex_t* vertex);
```

## 描述

为后续绘制绑定顶点（模型）资源

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | — |
| `vertex` | `je_gl_vertex_t*` | — |

## 返回值

无返回值。

## 注意事项

- 必须在 pass 内调用；draw 之前必须同时绑定 pipeline 和 vertex

## 相关接口

- [je_graphic_cmd_bind_pipeline](je_graphic_cmd_bind_pipeline.md) - 为后续绘制绑定 pipeline
- [je_graphic_cmd_draw](je_graphic_cmd_draw.md) - 在当前 pipeline/vertex/绑定状态下发出绘制。
- [je_graphic_cmd_draw_indexed](je_graphic_cmd_draw_indexed.md) - 在当前 pipeline/vertex/绑定状态下发出绘制。
