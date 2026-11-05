# je_graphic_resource_handle_share

## 函数签名

```c
JE_API void je_graphic_resource_handle_share(je_gl_resource_handle_t* resource_handle);
```

## 描述

增加指定资源句柄的引用计数，使得对应的图形资源需要额外执行一次 `close` 操作才能被释放。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `resource_handle` | `je_gl_resource_handle_t*` | 资源句柄指针 |

## 返回值

无返回值。

## 用法

此函数通常用于实现资源的共享引用。

### 宏封装

```cpp
#define jegl_share_resource(resource) je_graphic_resource_handle_share(&(resource)->m_handle)
```

### 示例

```cpp
je_gl_texture_2d_t* texture = je_graphic_texture_2d_load(ctx, "@/textures/brick.png");

// 共享纹理给另一个对象
jegl_share_resource(texture);  // 引用计数 +1

// 现在需要调用两次 je_graphic_texture_2d_close 才能真正释放资源
je_graphic_texture_2d_close(texture);  // 引用计数 -1
je_graphic_texture_2d_close(texture);  // 引用计数 -1，资源被释放
```

## 注意事项

- 每次调用 `jegl_share_resource` 后，都需要额外调用一次对应的 `close` 函数
- 适用于所有图形资源：着色器、纹理、顶点、帧缓冲、一致变量缓冲

## 相关接口

- [je_graphic_shader_close](je_graphic_shader_close.md) - 关闭着色器
- [je_graphic_texture_2d_close](je_graphic_texture_2d_close.md) - 关闭纹理
- [je_graphic_vertex_close](je_graphic_vertex_close.md) - 关闭顶点
