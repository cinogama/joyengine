# jegl_using_metal_apis

## 函数签名

```c
JE_API void jegl_using_metal_apis(jegl_graphic_api* write_to_apis);
```

## 描述

获取 Metal 图形后端的 API 集合。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `write_to_apis` | `jegl_graphic_api*` | 用于写入 API 函数指针的结构体指针 |

## 返回值

无返回值。

## 用法

此函数用于初始化 Metal 图形后端（仅 macOS 和 iOS）。

### 示例

```cpp
// 创建使用 Metal 的图形线程
jegl_context* ctx = jegl_start_graphic_thread(
    config,
    universe,
    jegl_using_metal_apis,
    render_func,
    arg
);
```

## 注意事项

- 仅在 macOS 和 iOS 平台上可用
- Apple 平台上的推荐图形后端

## 相关接口

- [jegl_using_opengl3_apis](jegl_using_opengl3_apis.md) - OpenGL 3 后端
- [jegl_using_vk120_apis](jegl_using_vk120_apis.md) - Vulkan 1.2 后端
