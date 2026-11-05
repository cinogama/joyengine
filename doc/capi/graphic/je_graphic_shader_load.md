# je_graphic_shader_load

## 函数签名

```c
JE_API je_gl_shader_t* je_graphic_shader_load(
    je_gl_context_t* context,
    const char* path);
```

## 描述

从源码文件加载一个着色器实例，会创建或使用缓存文件以加速着色器的加载。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `context` | `je_gl_context_t*` | 图形上下文指针 |
| `path` | `const char*` | 着色器文件路径 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_shader_t*` | 着色器资源指针；若加载失败返回 `nullptr` |

## 用法

此函数用于从文件加载着色器。

### 示例

```cpp
je_gl_shader_t* shader = je_graphic_shader_load(ctx, "@/shaders/pbr.shader");

if (shader != nullptr) {
    // 使用着色器进行渲染...
    
    je_graphic_shader_close(shader);
}
```

## 注意事项

- 图形资源创建时自动获取一个引用计数
- 使用 `jegl_share_resource` 可增加引用计数
- 使用 `je_graphic_shader_close` 可减少引用计数
- 当引用计数变为 0 时，资源将被释放
- 自动创建和使用 `.je4cache` 缓存文件

## 相关接口

- [je_graphic_shader_source_load](je_graphic_shader_source_load.md) - 从源码加载着色器
- [je_graphic_shader_close](je_graphic_shader_close.md) - 关闭着色器
