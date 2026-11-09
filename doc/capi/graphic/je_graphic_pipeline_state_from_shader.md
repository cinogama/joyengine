# je_graphic_pipeline_state_from_shader

## 函数签名

```c
JE_API je_gl_pipeline_state_t je_graphic_pipeline_state_from_shader(
    je_gl_shader_t* shader);
```

## 描述

用 shader 的 ZTEST/ZWRITE/BLEND/CULL 配置填充一份 pipeline 状态。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `shader` | `je_gl_shader_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_pipeline_state_t` | 见描述。 |
