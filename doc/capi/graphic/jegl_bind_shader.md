# jegl_bind_shader

## 函数签名

```c
JE_API bool jegl_bind_shader(jegl_shader* shader);
```

## 描述

绑定着色器用于后续渲染。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `shader` | `jegl_shader*` | 着色器资源指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 绑定成功返回 `true`，失败返回 `false` |

## 用法

此函数用于设置当前渲染使用的着色器程序。

### 示例

```cpp
jegl_shader* shader = jegl_load_shader(ctx, "@/shaders/pbr.shader");

if (jegl_bind_shader(shader)) {
    // 设置 uniform 变量
    // 绑定纹理
    // 绘制顶点
    jegl_draw_vertex(mesh);
}
```

## 注意事项

- 绑定着色器后才能设置 uniform 变量和绘制
- 如果着色器无效可能返回 `false`

## 相关接口

- [jegl_load_shader](jegl_load_shader.md) - 加载着色器
- [jegl_set_uniform_value](jegl_set_uniform_value.md) - 设置 uniform 值
- [jegl_draw_vertex](jegl_draw_vertex.md) - 绘制顶点
