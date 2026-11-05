# je_graphic_pipeline_create

## 函数签名

```c
JE_API je_gl_pipeline_t* /* NOT NULL */ je_graphic_pipeline_create(
    je_gl_context_t* context, const je_gl_pipeline_desc_t* desc);
```

## 描述

创建一个 pipeline：shader + 固定功能状态 + 顶点布局兼容性。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `context` | `je_gl_context_t*` | — |
| `desc` | `const je_gl_pipeline_desc_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_pipeline_t*` | 见描述。 |

## 注意事项

- 返回的 pipeline 持有 shader 的引用
- 必须在对应图形线程的 context 生命周期内使用
