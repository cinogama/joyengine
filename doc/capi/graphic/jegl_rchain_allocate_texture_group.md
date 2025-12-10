# jegl_rchain_allocate_texture_group

## 函数签名

```c
JE_API jegl_rchain_texture_group* jegl_rchain_allocate_texture_group(
    jegl_rendchain* chain);
```

## 描述

创建纹理组，返回可用于绘制操作的纹理组句柄。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `chain` | `jegl_rendchain*` | 绘制链实例指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_rchain_texture_group*` | 纹理组句柄 |

## 用法

此函数用于创建纹理组，可通过 `jegl_rchain_bind_texture` 向纹理组中提交纹理。

### 示例

```cpp
jegl_rendchain* chain = jegl_rchain_create();
jegl_rchain_begin(chain, nullptr, 0, 0, width, height);

// 创建纹理组
jegl_rchain_texture_group* tex_group = jegl_rchain_allocate_texture_group(chain);

// 绑定纹理到纹理组
jegl_rchain_bind_texture(chain, tex_group, 0, diffuse_texture);
jegl_rchain_bind_texture(chain, tex_group, 1, normal_texture);

// 使用纹理组进行绘制
jegl_rchain_draw(chain, shader, mesh, tex_group);
```

## 注意事项

- 纹理组的生命周期与绘制链相同
- 若绘制的物体不需要使用纹理，可以传入 `nullptr` 给 `jegl_rchain_draw`

## 相关接口

- [jegl_rchain_bind_texture](jegl_rchain_bind_texture.md) - 绑定纹理到纹理组
- [jegl_rchain_draw](jegl_rchain_draw.md) - 绘制操作
