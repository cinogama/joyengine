# jegl_mark_shared_resources_outdated

## 函数签名

```c
JE_API bool jegl_mark_shared_resources_outdated(
    jegl_context* context,
    const char* path);
```

## 描述

标记指定路径的共享资源为过期状态。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `context` | `jegl_context*` | 图形上下文指针 |
| `path` | `const char*` | 资源路径 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 成功返回 `true`，失败返回 `false` |

## 用法

此函数用于在资源文件更新后，通知图形系统重新加载对应的资源。

### 示例

```cpp
// 当资源文件被修改后
jegl_mark_shared_resources_outdated(ctx, "@/shaders/pbr.shader");

// 下次使用该资源时会自动重新加载
```

## 注意事项

- 主要用于热重载功能
- 只对通过路径加载的共享资源有效

## 相关接口

- [jegl_load_shader](jegl_load_shader.md) - 加载着色器
- [jegl_load_texture](jegl_load_texture.md) - 加载纹理
