# je_graphic_cmd_draw_indexed

## 函数签名

```c
JE_API void je_graphic_cmd_draw_indexed(
    je_gl_command_buffer_t* cmd,
    uint32_t index_count, uint32_t first_index, uint32_t instance_count);
```

## 描述

在当前 pipeline/vertex/绑定状态下发出绘制。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | — |
| `index_count` | `uint32_t` | — |
| `first_index` | `uint32_t` | — |
| `instance_count` | `uint32_t` | — |

## 返回值

无返回值。

## 注意事项

- index_count/vertex_count/instance_count 必须大于 0
- 索引类型固定为 uint32_t
- Debug 构建校验 shader 的顶点输入能否由绑定的顶点布局提供、纹理类别是否与 shader 声明一致：不匹配时本次绘制会被丢弃，并输出一次错误日志（同一 shader/顶点组合只报告一次）。Release 构建不做这些校验（逐 draw 检查的开销只保留在调试期），错误的材质数据在 Release 下的渲染结果未定义
