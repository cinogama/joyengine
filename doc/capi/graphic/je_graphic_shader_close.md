# je_graphic_shader_close

## 函数签名

```c
JE_API void je_graphic_shader_close(je_gl_shader_t* shader);
```

## 描述

关闭一个着色器资源（使得其引用计数减少）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `shader` | `je_gl_shader_t*` | 着色器资源指针 |

## 返回值

无返回值。

## 用法

此函数用于释放着色器资源。

### 示例

```cpp
je_gl_shader_t* shader = je_graphic_shader_load(ctx, "@/shaders/basic.shader");

// 使用着色器...

// 释放资源
je_graphic_shader_close(shader);
```

## 注意事项

- 如果资源被 `jegl_share_resource` 引用过多次，需要对应调用多次才能真正释放资源
- 如果此操作使得资源的引用计数变为 0，资源应当被立即视为不可用
- 对应的底层图形资源将由资源对应的图形线程上下文在适当的时机释放

## 相关接口

- [je_graphic_shader_load](je_graphic_shader_load.md) - 加载着色器
- [je_graphic_resource_handle_share](je_graphic_resource_handle_share.md) - 增加资源引用计数
