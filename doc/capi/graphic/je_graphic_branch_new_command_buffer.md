# je_graphic_branch_new_command_buffer

## 函数签名

```c
JE_API je_gl_command_buffer_t* /* NOT NULL */ je_graphic_branch_new_command_buffer(
    je_render_branch_t* branch,
    /* OPTIONAL: nullptr 表示屏幕 */ je_gl_frame_buffer_t* framebuffer,
    const je_gl_viewport_t* viewport);
```

## 描述

从绘制组申请一个已经 reset 的命令缓冲区；屏幕目标传 nullptr。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `branch` | `je_render_branch_t*` | — |
| `framebuffer` | `je_gl_frame_buffer_t*` | 可选（nullptr 表示屏幕） |
| `viewport` | `const je_gl_viewport_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_command_buffer_t*` | 见描述。 |
