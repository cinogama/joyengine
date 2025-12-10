# jegl_using_opengl3_apis

## 函数签名

```c
JE_API void jegl_using_opengl3_apis(jegl_graphic_api* write_to_apis);
```

## 描述

获取 OpenGL 3 图形后端的 API 集合。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `write_to_apis` | `jegl_graphic_api*` | 用于写入 API 函数指针的结构体指针 |

## 返回值

无返回值。

## 用法

此函数用于初始化 OpenGL 3 图形后端。

### 示例

```cpp
// 创建使用 OpenGL 3 的图形线程
jegl_context* ctx = jegl_start_graphic_thread(
    config,
    universe,
    jegl_using_opengl3_apis,
    render_func,
    arg
);
```

## 注意事项

- 需要系统支持 OpenGL 3.3 或更高版本
- 跨平台兼容性较好

## 相关接口

- [jegl_using_vk120_apis](jegl_using_vk120_apis.md) - Vulkan 1.2 后端
- [jegl_using_dx11_apis](jegl_using_dx11_apis.md) - DirectX 11 后端
- [jegl_using_metal_apis](jegl_using_metal_apis.md) - Metal 后端
