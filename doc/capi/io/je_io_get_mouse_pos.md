# je_io_get_mouse_pos

## 函数签名

```cpp
JE_API void je_io_get_mouse_pos(size_t group, int* out_x, int* out_y);
```

## 描述

获取鼠标的坐标。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| group | size_t | 鼠标/触摸点的分组索引 |
| out_x | int* | 输出参数，接收鼠标的 X 坐标 |
| out_y | int* | 输出参数，接收鼠标的 Y 坐标 |

## 返回值

无（通过输出参数返回）

## 用法示例

```cpp
int mouseX, mouseY;
je_io_get_mouse_pos(0, &mouseX, &mouseY);
// mouseX 和 mouseY 现在包含主鼠标的坐标
```

## 注意事项

- group 参数支持多点触控场景
- 坐标值通过 `je_io_update_mouse_pos` 设置

## 相关接口

- [je_io_update_mouse_pos](je_io_update_mouse_pos.md) - 更新鼠标位置
