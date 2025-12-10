# je_io_update_mouse_pos

## 函数签名

```cpp
JE_API void je_io_update_mouse_pos(size_t group, int x, int y);
```

## 描述

更新鼠标（或触摸位置点）的坐标。此操作**不会**影响光标的实际位置，仅用于更新引擎内部记录的鼠标坐标状态。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| group | size_t | 鼠标/触摸点的分组索引，用于支持多点触控 |
| x | int | 鼠标/触摸点的 X 坐标 |
| y | int | 鼠标/触摸点的 Y 坐标 |

## 返回值

无

## 用法示例

```cpp
// 更新主鼠标（group 0）的位置
je_io_update_mouse_pos(0, 100, 200);

// 更新第二个触摸点的位置
je_io_update_mouse_pos(1, 300, 400);
```

## 注意事项

- 此函数是基本接口，用于由图形后端或平台层向引擎报告鼠标位置
- 此操作不会移动实际的光标位置
- group 参数支持多点触控场景，不同的 group 代表不同的触摸点

## 相关接口

- [je_io_get_mouse_pos](je_io_get_mouse_pos.md) - 获取鼠标的坐标
- [je_io_update_mouse_state](je_io_update_mouse_state.md) - 更新鼠标按键状态
