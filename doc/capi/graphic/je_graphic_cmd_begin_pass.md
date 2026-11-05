# je_graphic_cmd_begin_pass

## 函数签名

```c
JE_API void je_graphic_cmd_begin_pass(
    je_gl_command_buffer_t* cmd,
    /* OPTIONAL: nullptr 表示屏幕后备缓冲区 */ je_gl_frame_buffer_t* target,
    const je_gl_viewport_t* viewport);
```

## 描述

开始一个渲染 pass。target 为空表示屏幕后备缓冲区。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | — |
| `target` | `je_gl_frame_buffer_t*` | 可选（nullptr 表示屏幕后备缓冲区） |
| `viewport` | `const je_gl_viewport_t*` | viewport 的 width/height 必须大于 0 |

## 返回值

无返回值。

## 注意事项

- 不允许嵌套 pass
- viewport 的 width/height 必须大于 0
