# jegl_bind_uniform_buffer

## 函数签名

```c
JE_API void jegl_bind_uniform_buffer(jegl_uniform_buffer* uniformbuf);
```

## 描述

绑定一致变量缓冲区。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `uniformbuf` | `jegl_uniform_buffer*` | 一致变量缓冲区资源指针 |

## 返回值

无返回值。

## 用法

此函数用于将 UBO 绑定到其创建时指定的绑定位置。

### 示例

```cpp
// 创建并更新 UBO
jegl_uniform_buffer* camera_ubo = jegl_create_uniformbuf(0, sizeof(CameraData));
jegl_update_uniformbuf(camera_ubo, &camera_data, 0, sizeof(CameraData));

// 绑定 UBO
jegl_bind_uniform_buffer(camera_ubo);

// 绑定着色器并绘制
jegl_bind_shader(shader);
jegl_draw_vertex(mesh);
```

## 注意事项

- UBO 会绑定到创建时指定的 `binding_place`
- 绑定的 UBO 在多次绘制调用中保持有效

## 相关接口

- [jegl_create_uniformbuf](jegl_create_uniformbuf.md) - 创建一致变量缓冲
- [jegl_update_uniformbuf](jegl_update_uniformbuf.md) - 更新一致变量缓冲
