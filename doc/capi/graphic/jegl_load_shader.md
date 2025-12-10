# jegl_load_shader

## 函数签名

```c
JE_API jegl_shader* jegl_load_shader(
    jegl_context* context,
    const char* path);
```

## 描述

从源码文件加载一个着色器实例，会创建或使用缓存文件以加速着色器的加载。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `context` | `jegl_context*` | 图形上下文指针 |
| `path` | `const char*` | 着色器文件路径 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_shader*` | 着色器资源指针；若加载失败返回 `nullptr` |

## 用法

此函数用于从文件加载着色器。

### 示例

```cpp
jegl_shader* shader = jegl_load_shader(ctx, "@/shaders/pbr.shader");

if (shader != nullptr) {
    // 使用着色器进行渲染...
    
    jegl_close_shader(shader);
}
```

## 注意事项

- 图形资源创建时自动获取一个引用计数
- 使用 `jegl_share_resource` 可增加引用计数
- 使用 `jegl_close_shader` 可减少引用计数
- 当引用计数变为 0 时，资源将被释放
- 自动创建和使用 `.je4cache` 缓存文件

## 相关接口

- [jegl_load_shader_source](jegl_load_shader_source.md) - 从源码加载着色器
- [jegl_close_shader](jegl_close_shader.md) - 关闭着色器
