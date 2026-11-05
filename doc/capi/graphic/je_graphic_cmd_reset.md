# je_graphic_cmd_reset

## 函数签名

```c
JE_API void je_graphic_cmd_reset(jegl_command_buffer* cmd);
```

## 描述

重置命令缓冲区以便复用：清空已录制的命令，并释放录制期持有的全部资源引用

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `jegl_command_buffer*` | — |

## 返回值

无返回值。

## 注意事项

- 重置会直接丢弃仍未提交的内容（包括尚未结束的 pass），不会报错

## 相关接口

- [je_graphic_cmd_create](je_graphic_cmd_create.md) - 创建一个命令缓冲区，并绑定到指定的图形上下文
- [je_graphic_cmd_commit](je_graphic_cmd_commit.md) - 在图形线程提交命令缓冲区。
