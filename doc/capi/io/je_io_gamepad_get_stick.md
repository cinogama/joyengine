# je_io_gamepad_get_stick

## 函数签名

```cpp
JE_API void je_io_gamepad_get_stick(
    je_io_gamepad_handle_t gamepad, jeecs::input::joystickcode stickid, float* out_x, float* out_y);
```

## 描述

获取指定虚拟手柄的指定摇杆的坐标。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| gamepad | je_io_gamepad_handle_t | 虚拟手柄句柄 |
| stickid | jeecs::input::joystickcode | 摇杆枚举值 |
| out_x | float* | 输出参数，接收摇杆 X 坐标 |
| out_y | float* | 输出参数，接收摇杆 Y 坐标 |

## 返回值

无（通过输出参数返回）

## 用法示例

```cpp
float x, y;

// 获取左摇杆位置
je_io_gamepad_get_stick(gamepad, jeecs::input::joystickcode::L_STICK, &x, &y);
// x 和 y 的模长范围是 [0, 1]

// 获取左扳机键（LT）
je_io_gamepad_get_stick(gamepad, jeecs::input::joystickcode::LT, &x, &y);
// x 值表示扳机按下程度，y 值始终为 0
```

## 注意事项

- 坐标的模长取值范围是 [0, 1]
- 如果手柄已经被断开（`je_io_close_gamepad`），调用此接口依然是合法的，但获取到的坐标都将是 0
- 获取扳机键（LT, RT）时，请使用 x 值，y 值始终为 0

## 相关接口

- [je_io_gamepad_update_stick](je_io_gamepad_update_stick.md) - 更新摇杆坐标
- [je_io_gamepad_stick_set_deadzone](je_io_gamepad_stick_set_deadzone.md) - 设置摇杆死区
- [je_io_close_gamepad](je_io_close_gamepad.md) - 关闭虚拟手柄实例
