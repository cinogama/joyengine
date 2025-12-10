# jegl_close_shader

## 函数签名

```c
JE_API void jegl_close_shader(jegl_shader* shader);
```

## 描述

关闭一个着色器资源（使得其引用计数减少）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `shader` | `jegl_shader*` | 着色器资源指针 |

## 返回值

无返回值。

## 用法

此函数用于释放着色器资源。

### 示例

```cpp
jegl_shader* shader = jegl_load_shader(ctx, "@/shaders/basic.shader");

// 使用着色器...

// 释放资源
jegl_close_shader(shader);
```

## 注意事项

- 如果资源被 `jegl_share_resource` 引用过多次，需要对应调用多次才能真正释放资源
- 如果此操作使得资源的引用计数变为 0，资源应当被立即视为不可用
- 对应的底层图形资源将由资源对应的图形线程上下文在适当的时机释放

## 相关接口

- [jegl_load_shader](jegl_load_shader.md) - 加载着色器
- [jegl_share_resource_handle](jegl_share_resource_handle.md) - 增加资源引用计数
