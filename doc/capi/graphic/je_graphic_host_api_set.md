# je_graphic_host_api_set

## 函数签名

```c
JE_API void je_graphic_host_api_set(jegl_graphic_api_entry api);
```

## 描述

设置宿主图形 API。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `api` | `jegl_graphic_api_entry` | 图形 API 入口函数指针 |

## 返回值

无返回值。

## 用法

此函数用于设置全局的图形 API 后端。

### 示例

```cpp
// 设置使用 OpenGL 3 作为图形后端
je_graphic_host_api_set(je_graphic_using_opengl3_apis);

// 或设置使用 Vulkan 1.2
je_graphic_host_api_set(je_graphic_using_vk120_apis);
```

## 相关接口

- [je_graphic_host_api_get](je_graphic_host_api_get.md) - 获取宿主图形 API
- [je_graphic_using_opengl3_apis](je_graphic_using_opengl3_apis.md) - OpenGL 3 后端
- [je_graphic_using_vk120_apis](je_graphic_using_vk120_apis.md) - Vulkan 1.2 后端
- [je_graphic_using_dx11_apis](je_graphic_using_dx11_apis.md) - DirectX 11 后端
