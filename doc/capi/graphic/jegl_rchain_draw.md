# jegl_rchain_draw

## 函数签名

```c
JE_API jegl_rendchain_rend_action* jegl_rchain_draw(
    jegl_rendchain* chain,
    jegl_shader* shader,
    jegl_vertex* vertex,
    jegl_rchain_texture_group* texture_group_may_null);
```

## 描述

将指定的顶点，使用指定的着色器和纹理将绘制操作作用到绘制链上。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `chain` | `jegl_rendchain*` | 绘制链实例指针 |
| `shader` | `jegl_shader*` | 着色器资源指针 |
| `vertex` | `jegl_vertex*` | 顶点资源指针 |
| `texture_group_may_null` | `jegl_rchain_texture_group*` | 纹理组（可为 `nullptr`） |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_rendchain_rend_action*` | 绘制操作对象指针，用于后续设置 uniform 变量 |

## 用法

此函数用于向绘制链添加一次绘制操作。

### 示例

```cpp
// 创建纹理组并绑定纹理
jegl_rchain_texture_group* tex_group = jegl_rchain_allocate_texture_group(chain);
jegl_rchain_bind_texture(chain, tex_group, 0, texture);

// 添加绘制操作
jegl_rendchain_rend_action* act = jegl_rchain_draw(chain, shader, mesh, tex_group);

// 设置 uniform 变量
jegl_rchain_set_uniform_float4x4(act, nullptr, model_matrix);
jegl_rchain_set_uniform_float4(act, &color_location, color);
```

## 注意事项

- 若绘制的物体不需要使用纹理，可以使用不绑定纹理的纹理组或传入 `nullptr`
- 返回的对象仅限在同一个渲染链的下一次绘制命令开始之前使用

## 相关接口

- [jegl_rchain_allocate_texture_group](jegl_rchain_allocate_texture_group.md) - 分配纹理组
- [jegl_rchain_set_uniform_float4x4](jegl_rchain_set_uniform_float4x4.md) - 设置矩阵 uniform
