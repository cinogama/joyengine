# je_graphic_cmd_get_context

## 函数签名

```c
JE_API je_gl_context_t* /* NOT NULL */ je_graphic_cmd_get_context(
    je_gl_command_buffer_t* cmd);
```

## 描述

返回创建命令缓冲区时绑定的图形上下文

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_context_t*` | 见描述。 |

## 注意事项

- 该上下文也是唯一允许调用 je_graphic_cmd_commit 的图形线程

## 相关接口

- [je_graphic_cmd_create](je_graphic_cmd_create.md) - 创建一个命令缓冲区，并绑定到指定的图形上下文
- [je_graphic_cmd_commit](je_graphic_cmd_commit.md) - 在图形线程提交命令缓冲区。
