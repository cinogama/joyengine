# jegl_close_texture

## 函数签名

```c
JE_API void jegl_close_texture(jegl_texture* texture);
```

## 描述

关闭一个纹理资源（使得其引用计数减少）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `texture` | `jegl_texture*` | 纹理资源指针 |

## 返回值

无返回值。

## 用法

此函数用于释放纹理资源。

### 示例

```cpp
jegl_texture* texture = jegl_load_texture(ctx, "@/textures/diffuse.png");

// 使用纹理...

// 释放资源
jegl_close_texture(texture);
```

## 注意事项

- 如果资源被 `jegl_share_resource` 引用过多次，需要对应调用多次才能真正释放资源
- 如果此操作使得资源的引用计数变为 0，资源应当被立即视为不可用
- 对应的底层图形资源将由资源对应的图形线程上下文在适当的时机释放

## 相关接口

- [jegl_load_texture](jegl_load_texture.md) - 加载纹理
- [jegl_create_texture](jegl_create_texture.md) - 创建纹理
- [jegl_share_resource_handle](jegl_share_resource_handle.md) - 增加资源引用计数
