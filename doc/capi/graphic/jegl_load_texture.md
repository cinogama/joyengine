# jegl_load_texture

## 函数签名

```c
JE_API jegl_texture* jegl_load_texture(
    jegl_context* context,
    const char* path);
```

## 描述

从指定路径加载一个纹理资源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `context` | `jegl_context*` | 图形上下文指针 |
| `path` | `const char*` | 纹理文件路径 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_texture*` | 纹理资源指针；若加载失败返回 `nullptr` |

## 用法

加载的路径规则与 `jeecs_file_open` 相同。

### 示例

```cpp
jegl_texture* texture = jegl_load_texture(ctx, "@/textures/brick.png");

if (texture != nullptr) {
    // 绑定纹理到着色器
    jegl_bind_texture(texture, 0);
    
    // 使用完毕后释放
    jegl_close_texture(texture);
}
```

## 注意事项

- 若指定的文件不存在或不是一个合法的纹理，则返回 `nullptr`
- 图形资源创建时自动获取一个引用计数
- 使用 `jegl_share_resource` 可增加引用计数
- 使用 `jegl_close_texture` 可减少引用计数

## 相关接口

- [jegl_create_texture](jegl_create_texture.md) - 创建纹理
- [jegl_close_texture](jegl_close_texture.md) - 关闭纹理
- [jeecs_file_open](../file/jeecs_file_open.md) - 打开文件
