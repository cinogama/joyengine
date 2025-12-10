# jegl_rchain_bind_texture

## 函数签名

```c
JE_API void jegl_rchain_bind_texture(
    jegl_rendchain* chain,
    jegl_rchain_texture_group* texture_group,
    size_t binding_pass,
    jegl_texture* texture);
```

## 描述

为指定的纹理组，在指定的纹理通道绑定一个纹理。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `chain` | `jegl_rendchain*` | 绘制链实例指针 |
| `texture_group` | `jegl_rchain_texture_group*` | 纹理组句柄 |
| `binding_pass` | `size_t` | 纹理绑定通道 |
| `texture` | `jegl_texture*` | 纹理资源指针 |

## 返回值

无返回值。

## 用法

此函数用于向纹理组中添加纹理绑定。

### 示例

```cpp
jegl_rchain_texture_group* tex_group = jegl_rchain_allocate_texture_group(chain);

// 绑定漫反射纹理到通道 0
jegl_rchain_bind_texture(chain, tex_group, 0, diffuse_texture);

// 绑定法线纹理到通道 1
jegl_rchain_bind_texture(chain, tex_group, 1, normal_texture);

// 绑定高光纹理到通道 2
jegl_rchain_bind_texture(chain, tex_group, 2, specular_texture);
```

## 注意事项

- `binding_pass` 对应着色器中的采样器绑定位置

## 相关接口

- [jegl_rchain_allocate_texture_group](jegl_rchain_allocate_texture_group.md) - 分配纹理组
- [jegl_rchain_draw](jegl_rchain_draw.md) - 绘制操作
