# je_gui_backend_init

## 函数签名

```c
JE_API void je_gui_backend_init(
    je_gl_context_t* gl_context,
    je_gui_user_image_loader_t get_img_res,
    je_gui_user_sampler_loader_t apply_shader_sampler);
```

## 描述

初始化 ImGUI。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `gl_context` | `je_gl_context_t*` | 引擎图形上下文指针 |
| `get_img_res` | `je_gui_user_image_loader_t` | 获取纹理底层句柄的回调函数 |
| `apply_shader_sampler` | `je_gui_user_sampler_loader_t` | 应用着色器采样设置的回调函数 |

## 返回值

无返回值。

## 用法

此函数用于初始化 ImGUI 系统。

### 回调函数类型

```cpp
typedef uint64_t je_gui_user_image_handle_t;
typedef je_gui_user_image_handle_t(*je_gui_user_image_loader_t)(je_gl_context_t*, je_gl_texture_2d_t*);
typedef void (*je_gui_user_sampler_loader_t)(je_gl_context_t*, je_gl_shader_t*);
```

### 示例

```cpp
je_gui_user_image_handle_t my_image_loader(je_gl_context_t* ctx, je_gl_texture_2d_t* tex) {
    // 返回纹理的底层图形库句柄
    return (je_gui_user_image_handle_t)tex->m_resource_id;
}

void my_sampler_loader(je_gl_context_t* ctx, je_gl_shader_t* shader) {
    // 应用着色器的采样器设置
}

// 初始化
je_gui_backend_init(gl_context, my_image_loader, my_sampler_loader);
```

## 注意事项

- 此接口仅适合用于对接自定义渲染 API 时使用
- 必须在 `je_gui_backend_font_set` 之后调用（如果需要自定义字体）

## 相关接口

- [je_gui_backend_font_set](je_gui_backend_font_set.md) - 设置字体
- [je_gui_backend_update](je_gui_backend_update.md) - 更新 ImGUI
- [je_gui_backend_shutdown](je_gui_backend_shutdown.md) - 关闭 ImGUI
