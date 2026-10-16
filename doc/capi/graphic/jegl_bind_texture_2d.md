# jegl_bind_texture

## 函数签名

```c
JE_API void jegl_bind_texture_2d(jegl_texture_2d* texture, size_t pass);
```

## 描述

将纹理绑定到指定的纹理单元。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `texture` | `jegl_texture_2d*` | 纹理资源指针 |
| `pass` | `size_t` | 纹理单元索引 |

## 返回值

无返回值。

## 用法

此函数用于将纹理绑定到着色器可访问的纹理单元。

### 示例

```cpp
jegl_texture_2d* diffuse = jegl_load_texture_2d(ctx, "@/textures/diffuse.png");
jegl_texture_2d* normal = jegl_load_texture_2d(ctx, "@/textures/normal.png");

// 绑定漫反射纹理到单元 0
jegl_bind_texture_2d(diffuse, 0);

// 绑定法线纹理到单元 1
jegl_bind_texture_2d(normal, 1);

// 渲染...
```

## 注意事项

- `pass` 对应着色器中的采样器绑定位置
- 可用的纹理单元数量取决于硬件和图形 API

## 相关接口

- [jegl_load_texture](jegl_load_texture_2d.md) - 加载纹理
- [jegl_bind_shader](jegl_bind_shader.md) - 绑定着色器
