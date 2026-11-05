# je_graphic_pipeline_create

## 函数签名

```c
JE_API jegl_pipeline* /* NOT NULL */ je_graphic_pipeline_create(
    jegl_context* context, const jegl_pipeline_desc* desc);
```

## 描述

创建一个 pipeline：shader + 固定功能状态 + 顶点布局兼容性。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `context` | `jegl_context*` | — |
| `desc` | `const jegl_pipeline_desc*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_pipeline*` | 见描述。 |

## 注意事项

- 返回的 pipeline 持有 shader 的引用
- 必须在对应图形线程的 context 生命周期内使用
