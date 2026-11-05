# je_graphic_cmd_bind_uniform_buffer

## 函数签名

```c
JE_API void je_graphic_cmd_bind_uniform_buffer(
    je_gl_command_buffer_t* cmd, je_gl_uniform_buffer_t* buffer);
```

## 描述

为后续绘制绑定一致变量缓冲区

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | — |
| `buffer` | `je_gl_uniform_buffer_t*` | — |

## 返回值

无返回值。

## 注意事项

- 必须在 pass 内调用
- 使用 buffer 创建时指定的 binding place，避免绑定位置不一致

## 相关接口

- [je_graphic_uniformbuf_create](je_graphic_uniformbuf_create.md) - 创建一个指定大小和绑定位置的一致变量缓冲区资源
- [je_graphic_uniformbuf_update](je_graphic_uniformbuf_update.md) - 更新一个一致变量缓冲区中，指定位置起，若干长度的数据
