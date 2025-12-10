# je_io_fetch_update_window_size

## 函数签名

```cpp
JE_API bool je_io_fetch_update_window_size(int* out_x, int* out_y);
```

## 描述

获取当前窗口大小是否应该调整及调整的大小。此函数供图形后端调用，以检查是否有窗口大小调整请求。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| out_x | int* | 输出参数，接收目标窗口宽度 |
| out_y | int* | 输出参数，接收目标窗口高度 |

## 返回值

| 类型 | 描述 |
|------|------|
| bool | true 表示有调整请求，false 表示无请求 |

## 用法示例

```cpp
int newWidth, newHeight;
if (je_io_fetch_update_window_size(&newWidth, &newHeight)) {
    // 有窗口大小调整请求，执行调整
    // ... 调整窗口大小为 newWidth x newHeight
}
```

## 注意事项

- 此函数是一次性获取，调用后请求会被清除
- 通常由图形后端在主循环中调用

## 相关接口

- [je_io_set_window_size](je_io_set_window_size.md) - 请求调整窗口大小
