# je_io_set_window_title

## 函数签名

```cpp
JE_API void je_io_set_window_title(const char* title);
```

## 描述

请求对窗口标题进行调整。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| title | const char* | 目标窗口标题字符串 |

## 返回值

无

## 用法示例

```cpp
// 请求将窗口标题设置为 "My Game"
je_io_set_window_title("My Game");

// 设置带版本号的标题
je_io_set_window_title("My Game v1.0.0");
```

## 注意事项

- 此函数发出调整请求，实际调整由图形后端在适当时机执行
- 图形后端应通过 `je_io_fetch_update_window_title` 获取请求并执行

## 相关接口

- [je_io_fetch_update_window_title](je_io_fetch_update_window_title.md) - 获取窗口标题调整请求
