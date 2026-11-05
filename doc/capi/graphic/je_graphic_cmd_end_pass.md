# je_graphic_cmd_end_pass

## 函数签名

```c
JE_API void je_graphic_cmd_end_pass(jegl_command_buffer* cmd);
```

## 描述

结束当前渲染 pass

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `jegl_command_buffer*` | — |

## 返回值

无返回值。

## 注意事项

- 必须与 je_graphic_cmd_begin_pass 配对；未显式调用时 je_graphic_cmd_commit 会自动结束

## 相关接口

- [je_graphic_cmd_begin_pass](je_graphic_cmd_begin_pass.md) - 开始一个渲染 pass。target 为空表示屏幕后备缓冲区。
- [je_graphic_cmd_commit](je_graphic_cmd_commit.md) - 在图形线程提交命令缓冲区。
