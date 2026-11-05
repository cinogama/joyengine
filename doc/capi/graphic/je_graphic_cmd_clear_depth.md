# je_graphic_cmd_clear_depth

## 函数签名

```c
JE_API void je_graphic_cmd_clear_depth(
    jegl_command_buffer* cmd, float depth);
```

## 描述

只能在 begin_pass 之后、该 pass 的第一次 draw 之前调用。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `jegl_command_buffer*` | — |
| `depth` | `float` | — |

## 返回值

无返回值。
