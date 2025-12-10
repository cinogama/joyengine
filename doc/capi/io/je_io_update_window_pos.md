# je_io_update_window_pos

## 函数签名

```cpp
JE_API void je_io_update_window_pos(int x, int y);
```

## 描述

更新窗口位置。此操作**不会**影响窗口的实际位置，仅用于更新引擎内部记录的窗口位置状态。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| x | int | 窗口 X 坐标 |
| y | int | 窗口 Y 坐标 |

## 返回值

无

## 用法示例

```cpp
// 更新内部记录的窗口位置为 (100, 100)
je_io_update_window_pos(100, 100);
```

## 注意事项

- 此函数是基本接口，用于由图形后端或平台层向引擎报告窗口位置变化
- 此操作不会改变实际窗口的位置

## 相关接口

- [je_io_get_window_pos](je_io_get_window_pos.md) - 获取窗口的位置
