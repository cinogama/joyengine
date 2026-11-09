# je_graphic_cmd_set_uniform

## 函数签名

```c
JE_API void je_graphic_cmd_set_uniform(
    je_gl_command_buffer_t* cmd, je_gl_uniform_slot_t slot,
    const void* data, uint32_t size);
```

## 描述

设置当前 pipeline 的 per-draw uniform。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | — |
| `slot` | `je_gl_uniform_slot_t` | — |
| `data` | `const void*` | — |
| `size` | `uint32_t` | size 必须等于 slot.m_size；原始接口要求 data 非空 |

## 返回值

无返回值。

## 注意事项

- 必须先绑定 pipeline
- slot.m_offset 不得为 JE_GL_UNIFORM_SLOT_NONE
- size 必须等于 slot.m_size；原始接口要求 data 非空
