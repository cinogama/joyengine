# je_graphic_cmd_clear_color

## 函数签名

```c
JE_API void je_graphic_cmd_clear_color(
    jegl_command_buffer* cmd, uint32_t attachment, const float rgba[4]);
```

## 描述

只能在 begin_pass 之后、该 pass 的第一次 draw 之前调用。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `jegl_command_buffer*` | — |
| `attachment` | `uint32_t` | — |
| `rgba` | `const float` | — |

## 返回值

无返回值。
