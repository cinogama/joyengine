# je_graphic_using_dx11_apis

## 函数签名

```c
JE_API void je_graphic_using_dx11_apis(je_gl_graphic_api_t* write_to_apis);
```

## 描述

获取 DirectX 11 图形后端的 API 集合。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `write_to_apis` | `je_gl_graphic_api_t*` | 用于写入 API 函数指针的结构体指针 |

## 返回值

无返回值。

## 用法

此函数用于初始化 DirectX 11 图形后端（仅 Windows）。

### 示例

```cpp
// 创建使用 DirectX 11 的图形线程
je_gl_context_t* ctx = je_graphic_thread_start(
    config,
    universe,
    je_graphic_using_dx11_apis,
    render_func,
    arg
);
```

## 注意事项

- 仅在 Windows 平台上可用
- Windows 平台上的推荐图形后端

## 相关接口

- [je_graphic_using_opengl3_apis](je_graphic_using_opengl3_apis.md) - OpenGL 3 后端
- [je_graphic_using_vk120_apis](je_graphic_using_vk120_apis.md) - Vulkan 1.2 后端
