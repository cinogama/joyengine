# je_graphic_texture_2d_load

## 函数签名

```c
JE_API je_gl_texture_2d_t* je_graphic_texture_2d_load(
    je_gl_context_t* context,
    const char* path);
```

## 描述

从指定路径加载一个纹理资源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `context` | `je_gl_context_t*` | 图形上下文指针 |
| `path` | `const char*` | 纹理文件路径 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_texture_2d_t*` | 纹理资源指针；若加载失败返回 `nullptr` |

## 用法

加载的路径规则与 `je_file_open` 相同。

### 示例

```cpp
je_gl_texture_2d_t* texture = je_graphic_texture_2d_load(ctx, "@/textures/brick.png");

if (texture != nullptr) {
    // 在命令缓冲区中通过 je_graphic_cmd_bind_texture_2d 绑定纹理

    // 使用完毕后释放
    je_graphic_texture_2d_close(texture);
}
```

## 注意事项

- 从图片纹理缓存 `<path>.je4cache` 读取颜色空间与像素数据；缓存缺失或失效时先按图片与属性源文件 `<path>.je4texture` 重建缓存（导出的运行时只携带缓存、没有生成器）
- 若无法取得可用的缓存，则返回 `nullptr`
- 图形资源创建时自动获取一个引用计数
- 使用 `jegl_share_resource` 可增加引用计数
- 使用 `je_graphic_texture_2d_close` 可减少引用计数

## 相关接口

- [je_graphic_texture_2d_create](je_graphic_texture_2d_create.md) - 创建纹理
- [je_graphic_texture_2d_close](je_graphic_texture_2d_close.md) - 关闭纹理
- [je_image_texture_cache_open_or_create](je_image_texture_cache_open_or_create.md) - 打开或生成图片纹理缓存
- [je_file_open](../file/je_file_open.md) - 打开文件
