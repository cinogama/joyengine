# je_graphic_pipeline_builtin_uniform_slots

## 函数签名

```c
JE_API const je_gl_builtin_uniform_slots_t* /* NOT NULL */
je_graphic_pipeline_builtin_uniform_slots(je_gl_pipeline_t* pipeline);
```

## 描述

返回 pipeline 缓存的内建 uniform 位置表；指针由 pipeline 持有，生命周期同 pipeline。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `pipeline` | `je_gl_pipeline_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `const je_gl_builtin_uniform_slots_t*` | 见描述。 |
