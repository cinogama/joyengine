# je_io_gamepad_update_stick

## 函数签名

```cpp
JE_API void je_io_gamepad_update_stick(
    je_io_gamepad_handle_t gamepad, jeecs::input::joystickcode stickid, float x, float y);
```

## 描述

更新指定虚拟手柄的指定摇杆的坐标。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| gamepad | je_io_gamepad_handle_t | 虚拟手柄句柄 |
| stickid | jeecs::input::joystickcode | 摇杆枚举值 |
| x | float | 摇杆 X 坐标 |
| y | float | 摇杆 Y 坐标 |

## 返回值

无

## 用法示例

```cpp
// 更新左摇杆位置
je_io_gamepad_update_stick(gamepad, jeecs::input::joystickcode::L_STICK, 0.5f, 0.3f);

// 更新左扳机键（LT）- 半按
je_io_gamepad_update_stick(gamepad, jeecs::input::joystickcode::LT, 0.5f, 0.0f);
```

## 注意事项

- 应当确保坐标的模长不大于 1，如果输入坐标的模长大于 1，坐标将被归一化
- 不允许对已经断开（`je_io_close_gamepad`）的手柄进行此操作
- 获取扳机键（LT, RT）时，请使用 x 值，并保持 y 值始终为 0
- 当摇杆向上拨动时，y 值应当为正数；当摇杆向右拨动时，x 值应当为正数
- 当扳机键按下时，x 值应当为正数

## 相关接口

- [je_io_gamepad_get_stick](je_io_gamepad_get_stick.md) - 获取摇杆坐标
- [je_io_gamepad_stick_set_deadzone](je_io_gamepad_stick_set_deadzone.md) - 设置摇杆死区
- [je_io_close_gamepad](je_io_close_gamepad.md) - 关闭虚拟手柄实例
