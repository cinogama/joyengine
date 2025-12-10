# je_io_gamepad_stick_set_deadzone

## 函数签名

```cpp
JE_API void je_io_gamepad_stick_set_deadzone(
    je_io_gamepad_handle_t gamepad, jeecs::input::joystickcode stickid, float deadzone);
```

## 描述

设置指定虚拟手柄的指定摇杆的死区。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| gamepad | je_io_gamepad_handle_t | 虚拟手柄句柄 |
| stickid | jeecs::input::joystickcode | 摇杆枚举值 |
| deadzone | float | 死区值，通常在 [0.0, 1.0] 范围内 |

## 返回值

无

## 用法示例

```cpp
// 设置左摇杆的死区为 0.1
je_io_gamepad_stick_set_deadzone(gamepad, jeecs::input::joystickcode::L_STICK, 0.1f);

// 设置右摇杆的死区为 0.15
je_io_gamepad_stick_set_deadzone(gamepad, jeecs::input::joystickcode::R_STICK, 0.15f);
```

## 注意事项

- 当设置摇杆的坐标（`je_io_gamepad_update_stick`）时，如果坐标的模长小于死区，则坐标将被视为 0
- 死区用于过滤摇杆的微小漂移，避免游戏中的意外输入

## 相关接口

- [je_io_gamepad_get_stick](je_io_gamepad_get_stick.md) - 获取摇杆坐标
- [je_io_gamepad_update_stick](je_io_gamepad_update_stick.md) - 更新摇杆坐标
