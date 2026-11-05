# je_graphic_pipeline_builtin_uniform_slots

## 函数签名

```c
JE_API const jegl_builtin_uniform_slots* /* NOT NULL */
je_graphic_pipeline_builtin_uniform_slots(jegl_pipeline* pipeline);
```

## 描述

返回 pipeline 缓存的内建 uniform 位置表；指针由 pipeline 持有，生命周期同 pipeline。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `pipeline` | `jegl_pipeline*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `const jegl_builtin_uniform_slots*` | 见描述。 |
