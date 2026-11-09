# je_graphic_cmd_close

## 函数签名

```c
JE_API void je_graphic_cmd_close(je_gl_command_buffer_t* cmd);
```

## 描述

关闭一个命令缓冲区，并释放其录制期持有的全部资源引用

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | — |

## 返回值

无返回值。

## 注意事项

- 尚未提交的录制内容会被直接丢弃

## 相关接口

- [je_graphic_cmd_create](je_graphic_cmd_create.md) - 创建一个命令缓冲区，并绑定到指定的图形上下文
