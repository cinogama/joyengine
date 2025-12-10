# jegl_draw_vertex

## 函数签名

```c
JE_API void jegl_draw_vertex(jegl_vertex* vert);
```

## 描述

绘制顶点（模型）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `vert` | `jegl_vertex*` | 顶点资源指针 |

## 返回值

无返回值。

## 用法

此函数用于使用当前绑定的着色器绘制顶点数据。

### 示例

```cpp
// 设置渲染状态
jegl_bind_shader(shader);
jegl_bind_texture(texture, 0);

// 设置 uniform
jegl_set_uniform_value(uniform_location, &mvp_matrix);

// 绘制
jegl_draw_vertex(mesh);
```

## 注意事项

- 绘制前需要先绑定着色器
- 顶点数据将按照创建时指定的图元类型进行绘制

## 相关接口

- [jegl_load_vertex](jegl_load_vertex.md) - 加载顶点
- [jegl_create_vertex](jegl_create_vertex.md) - 创建顶点
- [jegl_bind_shader](jegl_bind_shader.md) - 绑定着色器
