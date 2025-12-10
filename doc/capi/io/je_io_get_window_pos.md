# je_io_get_window_pos

## 函数签名

```cpp
JE_API void je_io_get_window_pos(int* out_x, int* out_y);
```

## 描述

获取窗口的位置。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| out_x | int* | 输出参数，接收窗口 X 坐标 |
| out_y | int* | 输出参数，接收窗口 Y 坐标 |

## 返回值

无（通过输出参数返回）

## 用法示例

```cpp
int posX, posY;
je_io_get_window_pos(&posX, &posY);
// posX 和 posY 现在包含窗口的位置
```

## 相关接口

- [je_io_update_window_pos](je_io_update_window_pos.md) - 更新窗口位置状态
