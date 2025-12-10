# je_io_get_window_size

## 函数签名

```cpp
JE_API void je_io_get_window_size(int* out_x, int* out_y);
```

## 描述

获取窗口的大小。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| out_x | int* | 输出参数，接收窗口宽度 |
| out_y | int* | 输出参数，接收窗口高度 |

## 返回值

无（通过输出参数返回）

## 用法示例

```cpp
int width, height;
je_io_get_window_size(&width, &height);
// width 和 height 现在包含窗口的尺寸
```

## 相关接口

- [je_io_update_window_size](je_io_update_window_size.md) - 更新窗口大小状态
- [je_io_set_window_size](je_io_set_window_size.md) - 请求调整窗口大小
