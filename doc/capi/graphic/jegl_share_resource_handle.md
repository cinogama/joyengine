# jegl_share_resource_handle

## 函数签名

```c
JE_API void jegl_share_resource_handle(jegl_resource_handle* resource_handle);
```

## 描述

增加指定资源句柄的引用计数，使得对应的图形资源需要额外执行一次 `close` 操作才能被释放。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `resource_handle` | `jegl_resource_handle*` | 资源句柄指针 |

## 返回值

无返回值。

## 用法

此函数通常用于实现资源的共享引用。

### 宏封装

```cpp
#define jegl_share_resource(resource) jegl_share_resource_handle(&(resource)->m_handle)
```

### 示例

```cpp
jegl_texture* texture = jegl_load_texture(ctx, "@/textures/brick.png");

// 共享纹理给另一个对象
jegl_share_resource(texture);  // 引用计数 +1

// 现在需要调用两次 jegl_close_texture 才能真正释放资源
jegl_close_texture(texture);  // 引用计数 -1
jegl_close_texture(texture);  // 引用计数 -1，资源被释放
```

## 注意事项

- 每次调用 `jegl_share_resource` 后，都需要额外调用一次对应的 `close` 函数
- 适用于所有图形资源：着色器、纹理、顶点、帧缓冲、一致变量缓冲

## 相关接口

- [jegl_close_shader](jegl_close_shader.md) - 关闭着色器
- [jegl_close_texture](jegl_close_texture.md) - 关闭纹理
- [jegl_close_vertex](jegl_close_vertex.md) - 关闭顶点
