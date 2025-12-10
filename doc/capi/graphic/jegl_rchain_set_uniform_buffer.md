# jegl_rchain_set_uniform_buffer

## 函数签名

```c
JE_API void jegl_rchain_set_uniform_buffer(
    jegl_rendchain_rend_action* act,
    jegl_uniform_buffer* uniform_buffer);
```

## 描述

为指定的绘制操作应用一致变量缓冲区。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `act` | `jegl_rendchain_rend_action*` | 绘制操作对象指针 |
| `uniform_buffer` | `jegl_uniform_buffer*` | 一致变量缓冲区指针 |

## 返回值

无返回值。

## 用法

此函数用于为单个绘制操作设置 UBO。

### 示例

```cpp
jegl_rendchain_rend_action* act = jegl_rchain_draw(chain, shader, mesh, tex_group);

// 为此绘制操作设置物体专用的 UBO
jegl_rchain_set_uniform_buffer(act, object_ubo);
```

## 注意事项

- 与 `jegl_rchain_bind_uniform_buffer` 不同，此函数只影响单个绘制操作

## 相关接口

- [jegl_rchain_bind_uniform_buffer](jegl_rchain_bind_uniform_buffer.md) - 为整个链绑定 UBO
- [jegl_rchain_draw](jegl_rchain_draw.md) - 绘制操作
