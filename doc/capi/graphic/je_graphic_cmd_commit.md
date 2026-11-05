# je_graphic_cmd_commit

## 函数签名

```c
JE_API void je_graphic_cmd_commit(jegl_command_buffer* cmd);
```

## 描述

在图形线程提交命令缓冲区。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `jegl_command_buffer*` | — |

## 返回值

无返回值。

## 注意事项

- 若仍有未结束的 pass，commit 会自动结束它
- 提交完成后命令缓冲区保留已录制的命令，需要复用请先调用 je_graphic_cmd_reset
