# je_graphic_branch_new_command_buffer

## 函数签名

```c
JE_API jegl_command_buffer* /* NOT NULL */ je_graphic_branch_new_command_buffer(
    je_RenderBranch* branch,
    /* OPTIONAL: nullptr 表示屏幕 */ jegl_frame_buffer* framebuffer,
    const jegl_viewport* viewport);
```

## 描述

从绘制组申请一个已经 reset 的命令缓冲区；屏幕目标传 nullptr。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `branch` | `je_RenderBranch*` | — |
| `framebuffer` | `jegl_frame_buffer*` | 可选（nullptr 表示屏幕） |
| `viewport` | `const jegl_viewport*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_command_buffer*` | 见描述。 |
