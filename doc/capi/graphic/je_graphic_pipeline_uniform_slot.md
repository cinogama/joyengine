# je_graphic_pipeline_uniform_slot

## 函数签名

```c
JE_API /* OPTIONAL: m_offset == JEGL_UNIFORM_SLOT_NONE if absent */
jegl_uniform_slot je_graphic_pipeline_uniform_slot(
    jegl_pipeline* pipeline, const char* name);
```

## 描述

按名字查询 per-draw uniform 的规范化位置。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `pipeline` | `jegl_pipeline*` | — |
| `name` | `const char*` | name 不得为空 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_uniform_slot` | 见描述。 |

## 注意事项

- 返回值的 m_offset == JEGL_UNIFORM_SLOT_NONE 表示未声明
- name 不得为空
