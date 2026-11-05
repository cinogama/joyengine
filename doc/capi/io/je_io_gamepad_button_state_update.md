# je_io_gamepad_button_state_update

## 函数签名

```cpp
JE_API void je_io_gamepad_button_state_update(
    je_io_gamepad_handle_t gamepad, jeecs::input::gamepadcode code, bool down);
```

## 描述

更新指定的虚拟手柄按键状态，可以被 `je_io_gamepad_button_down_get` 获取。

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
je_io_gamepad_button_state_update(gamepad, jeecs::input::gamepadcode::A, true);

// 设置 A 按钮为释放状态
je_io_gamepad_button_state_update(gamepad, jeecs::input::gamepadcode::A, false);
```

## 注意事项

- 不允许对已经断开（`je_io_gamepad_close`）的手柄进行此操作

## 相关接口

- [je_io_gamepad_button_down_get](je_io_gamepad_button_down_get.md) - 获取手柄按键状态
- [je_io_gamepad_close](je_io_gamepad_close.md) - 关闭虚拟手柄实例
