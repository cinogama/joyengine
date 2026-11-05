# je_graphic_cmd_begin_pass

## 函数签名

```c
JE_API void je_graphic_cmd_begin_pass(
    jegl_command_buffer* cmd,
    /* OPTIONAL: nullptr 表示屏幕后备缓冲区 */ jegl_frame_buffer* target,
    const jegl_viewport* viewport);
```

## 描述

开始一个渲染 pass。target 为空表示屏幕后备缓冲区。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `jegl_command_buffer*` | — |
| `target` | `jegl_frame_buffer*` | 可选（nullptr 表示屏幕后备缓冲区） |
| `viewport` | `const jegl_viewport*` | viewport 的 width/height 必须大于 0 |

## 返回值

无返回值。

## 注意事项

- 不允许嵌套 pass
- viewport 的 width/height 必须大于 0
