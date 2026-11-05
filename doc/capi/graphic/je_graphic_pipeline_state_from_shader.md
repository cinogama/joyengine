# je_graphic_pipeline_state_from_shader

## 函数签名

```c
JE_API jegl_pipeline_state je_graphic_pipeline_state_from_shader(
    jegl_shader* shader);
```

## 描述

用 shader 的 ZTEST/ZWRITE/BLEND/CULL 配置填充一份 pipeline 状态。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `shader` | `jegl_shader*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_pipeline_state` | 见描述。 |
