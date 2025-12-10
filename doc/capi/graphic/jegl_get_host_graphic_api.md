# jegl_get_host_graphic_api

## 函数签名

```c
JE_API jegl_graphic_api_entry jegl_get_host_graphic_api(void);
```

## 描述

获取当前宿主图形 API。

## 参数

无参数。

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_graphic_api_entry` | 当前图形 API 入口函数指针 |

## 用法

此函数用于查询当前使用的图形 API 后端。

### 示例

```cpp
jegl_graphic_api_entry api = jegl_get_host_graphic_api();

if (api == jegl_using_opengl3_apis) {
    printf("Using OpenGL 3\n");
} else if (api == jegl_using_vk120_apis) {
    printf("Using Vulkan 1.2\n");
} else if (api == jegl_using_dx11_apis) {
    printf("Using DirectX 11\n");
}
```

## 相关接口

- [jegl_set_host_graphic_api](jegl_set_host_graphic_api.md) - 设置宿主图形 API
