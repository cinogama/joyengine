# je_io_set_window_size

## 函数签名

```cpp
JE_API void je_io_set_window_size(int x, int y);
```

## 描述

请求对窗口大小进行调整。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| x | int | 目标窗口宽度 |
| y | int | 目标窗口高度 |

## 返回值

无

## 用法示例

```cpp
// 请求将窗口大小调整为 1280x720
je_io_set_window_size(1280, 720);
```

## 注意事项

- 此函数发出调整请求，实际调整由图形后端在适当时机执行
- 图形后端应通过 `je_io_fetch_update_window_size` 获取请求并执行

## 相关接口

- [je_io_fetch_update_window_size](je_io_fetch_update_window_size.md) - 获取窗口大小调整请求
- [je_io_get_window_size](je_io_get_window_size.md) - 获取当前窗口大小
- [je_io_update_window_size](je_io_update_window_size.md) - 更新窗口大小状态
