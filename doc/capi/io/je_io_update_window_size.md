# je_io_update_window_size

## 函数签名

```cpp
JE_API void je_io_update_window_size(int x, int y);
```

## 描述

更新窗口大小。此操作**不会**影响窗口的实际大小，仅用于更新引擎内部记录的窗口尺寸状态。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| x | int | 窗口宽度 |
| y | int | 窗口高度 |

## 返回值

无

## 用法示例

```cpp
// 更新内部记录的窗口大小为 1920x1080
je_io_update_window_size(1920, 1080);
```

## 注意事项

- 此函数是基本接口，用于由图形后端或平台层向引擎报告窗口大小变化
- 此操作不会改变实际窗口的大小
- 通常在窗口大小改变事件回调中调用此函数

## 相关接口

- [je_io_get_window_size](je_io_get_window_size.md) - 获取窗口的大小
- [je_io_set_window_size](je_io_set_window_size.md) - 请求调整窗口大小
- [je_io_fetch_update_window_size](je_io_fetch_update_window_size.md) - 获取是否需要调整窗口大小
