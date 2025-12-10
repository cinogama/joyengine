# jegl_using_none_apis

## 函数签名

```c
JE_API void jegl_using_none_apis(jegl_graphic_api* write_to_apis);
```

## 描述

获取空图形后端的 API 集合。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `write_to_apis` | `jegl_graphic_api*` | 用于写入 API 函数指针的结构体指针 |

## 返回值

无返回值。

## 用法

此函数用于初始化空图形后端，适用于无头模式或测试环境。

### 示例

```cpp
jegl_graphic_api apis;
jegl_using_none_apis(&apis);

// 使用空后端创建图形线程（不会实际渲染）
jegl_context* ctx = jegl_start_graphic_thread(
    config,
    universe,
    jegl_using_none_apis,
    render_func,
    arg
);
```

## 注意事项

- 空后端不执行实际的图形渲染
- 主要用于测试或服务器环境

## 相关接口

- [jegl_using_opengl3_apis](jegl_using_opengl3_apis.md) - OpenGL 3 后端
- [jegl_using_vk120_apis](jegl_using_vk120_apis.md) - Vulkan 1.2 后端
