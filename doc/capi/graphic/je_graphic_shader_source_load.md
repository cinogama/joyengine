# je_graphic_shader_source_load

## 函数签名

```c
JE_API je_gl_shader_t* je_graphic_shader_source_load(
    je_gl_context_t* context,
    const char* path,
    const char* src,
    bool is_virtual_file);
```

## 描述

从源码加载一个着色器实例，可创建或使用缓存文件以加速着色器的加载。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `context` | `je_gl_context_t*` | 图形上下文指针 |
| `path` | `const char*` | 着色器路径（用于缓存和标识） |
| `src` | `const char*` | 着色器源代码 |
| `is_virtual_file` | `bool` | 是否为虚拟文件（是否创建缓存） |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_shader_t*` | 着色器资源指针；若加载失败返回 `nullptr` |

## 用法

此函数用于从内存中的源代码字符串加载着色器。

### 示例

```cpp
const char* shader_src = R"(
// JoyEngine Shader Source
import je/shader;

ZTEST   (LESS);
ZWRITE  (ENABLE);
BLEND   (SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
CULL    (BACK);

shader main
{
    // vertex shader
    // fragment shader
}
)";

je_gl_shader_t* shader = je_graphic_shader_source_load(
    ctx,
    "@/shaders/custom.shader",
    shader_src,
    false   // 不创建缓存文件
);

if (shader != nullptr) {
    // 使用着色器...
    je_graphic_shader_close(shader);
}
```

## 注意事项

- 图形资源创建时自动获取一个引用计数
- 使用 `jegl_share_resource` 可增加引用计数
- 使用 `je_graphic_shader_close` 可减少引用计数
- 当引用计数变为 0 时，资源将被释放
- 如果 `is_virtual_file` 为 `false`，则不会创建缓存文件

## 相关接口

- [je_graphic_shader_load](je_graphic_shader_load.md) - 从文件加载着色器
- [je_graphic_shader_close](je_graphic_shader_close.md) - 关闭着色器
- [je_graphic_resource_handle_share](je_graphic_resource_handle_share.md) - 增加资源引用计数
