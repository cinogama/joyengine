# je_graphic_host_api_get

## 函数签名

```c
JE_API je_gl_graphic_api_entry_t je_graphic_host_api_get(void);
```

## 描述

获取当前宿主图形 API。

## 参数

无参数。

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_graphic_api_entry_t` | 当前图形 API 入口函数指针 |

## 用法

此函数用于查询当前使用的图形 API 后端。

### 示例

```cpp
je_gl_graphic_api_entry_t api = je_graphic_host_api_get();

if (api == je_graphic_using_opengl3_apis) {
    printf("Using OpenGL 3\n");
} else if (api == je_graphic_using_vk120_apis) {
    printf("Using Vulkan 1.2\n");
} else if (api == je_graphic_using_dx11_apis) {
    printf("Using DirectX 11\n");
}
```

## 相关接口

- [je_graphic_host_api_set](je_graphic_host_api_set.md) - 设置宿主图形 API
