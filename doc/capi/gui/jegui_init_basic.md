# jegui_init_basic

## 函数签名

```c
JE_API void jegui_init_basic(
    jegl_context* gl_context,
    jegui_user_image_loader_t get_img_res,
    jegui_user_sampler_loader_t apply_shader_sampler);
```

## 描述

初始化 ImGUI。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `gl_context` | `jegl_context*` | 引擎图形上下文指针 |
| `get_img_res` | `jegui_user_image_loader_t` | 获取纹理底层句柄的回调函数 |
| `apply_shader_sampler` | `jegui_user_sampler_loader_t` | 应用着色器采样设置的回调函数 |

## 返回值

无返回值。

## 用法

此函数用于初始化 ImGUI 系统。

### 回调函数类型

```cpp
typedef uint64_t jegui_user_image_handle_t;
typedef jegui_user_image_handle_t(*jegui_user_image_loader_t)(jegl_context*, jegl_texture*);
typedef void (*jegui_user_sampler_loader_t)(jegl_context*, jegl_shader*);
```

### 示例

```cpp
jegui_user_image_handle_t my_image_loader(jegl_context* ctx, jegl_texture* tex) {
    // 返回纹理的底层图形库句柄
    return (jegui_user_image_handle_t)tex->m_resource_id;
}

void my_sampler_loader(jegl_context* ctx, jegl_shader* shader) {
    // 应用着色器的采样器设置
}

// 初始化
jegui_init_basic(gl_context, my_image_loader, my_sampler_loader);
```

## 注意事项

- 此接口仅适合用于对接自定义渲染 API 时使用
- 必须在 `jegui_set_font` 之后调用（如果需要自定义字体）

## 相关接口

- [jegui_set_font](jegui_set_font.md) - 设置字体
- [jegui_update_basic](jegui_update_basic.md) - 更新 ImGUI
- [jegui_shutdown_basic](jegui_shutdown_basic.md) - 关闭 ImGUI
