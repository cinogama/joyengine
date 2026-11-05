# je_graphic_cmd_create

## 函数签名

```c
JE_API jegl_command_buffer* /* NOT NULL */ je_graphic_cmd_create(
    jegl_context* context);
```

## 描述

创建一个命令缓冲区，并绑定到指定的图形上下文

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `context` | `jegl_context*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_command_buffer*` | 见描述。 |

## 注意事项

- 命令录制可以在任意线程进行；je_graphic_cmd_commit 只允许在 context 所属的图形线程调用
- 创建出的命令缓冲区需要使用 je_graphic_cmd_close 关闭

## 相关接口

- [je_graphic_cmd_close](je_graphic_cmd_close.md) - 关闭一个命令缓冲区，并释放其录制期持有的全部资源引用
- [je_graphic_cmd_reset](je_graphic_cmd_reset.md) - 重置命令缓冲区以便复用：清空已录制的命令，并释放录制期持有的全部资源引用
- [je_graphic_cmd_commit](je_graphic_cmd_commit.md) - 在图形线程提交命令缓冲区。
