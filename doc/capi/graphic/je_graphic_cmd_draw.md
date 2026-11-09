# je_graphic_cmd_draw

## 函数签名

```c
JE_API void je_graphic_cmd_draw(
    je_gl_command_buffer_t* cmd,
    uint32_t vertex_count, uint32_t first_vertex, uint32_t instance_count);
```

## 描述

在当前 pipeline/vertex/绑定状态下发出绘制。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | — |
| `vertex_count` | `uint32_t` | — |
| `first_vertex` | `uint32_t` | — |
| `instance_count` | `uint32_t` | — |

## 返回值

无返回值。

## 注意事项

- index_count/vertex_count/instance_count 必须大于 0
- 索引类型固定为 uint32_t
