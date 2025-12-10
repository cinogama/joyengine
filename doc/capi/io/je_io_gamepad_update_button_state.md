# je_io_gamepad_update_button_state

## 函数签名

```cpp
JE_API void je_io_gamepad_update_button_state(
    je_io_gamepad_handle_t gamepad, jeecs::input::gamepadcode code, bool down);
```

## 描述

更新指定的虚拟手柄按键状态，可以被 `je_io_gamepad_get_button_down` 获取。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| gamepad | je_io_gamepad_handle_t | 虚拟手柄句柄 |
| code | jeecs::input::gamepadcode | 手柄按键枚举值 |
| down | bool | true 表示按下，false 表示释放 |

## 返回值

无

## 用法示例

```cpp
// 设置 A 按钮为按下状态
je_io_gamepad_update_button_state(gamepad, jeecs::input::gamepadcode::A, true);

// 设置 A 按钮为释放状态
je_io_gamepad_update_button_state(gamepad, jeecs::input::gamepadcode::A, false);
```

## 注意事项

- 不允许对已经断开（`je_io_close_gamepad`）的手柄进行此操作

## 相关接口

- [je_io_gamepad_get_button_down](je_io_gamepad_get_button_down.md) - 获取手柄按键状态
- [je_io_close_gamepad](je_io_close_gamepad.md) - 关闭虚拟手柄实例
