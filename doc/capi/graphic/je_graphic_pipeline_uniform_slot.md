# je_graphic_pipeline_uniform_slot

## 函数签名

```c
JE_API /* OPTIONAL: m_offset == JE_GL_UNIFORM_SLOT_NONE if absent */
je_gl_uniform_slot_t je_graphic_pipeline_uniform_slot(
    je_gl_pipeline_t* pipeline, const char* name);
```

## 描述

按名字查询 per-draw uniform 的规范化位置。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `pipeline` | `je_gl_pipeline_t*` | — |
| `name` | `const char*` | name 不得为空 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_uniform_slot_t` | 见描述。 |

## 注意事项

- 返回值的 m_offset == JE_GL_UNIFORM_SLOT_NONE 表示未声明
- name 不得为空
