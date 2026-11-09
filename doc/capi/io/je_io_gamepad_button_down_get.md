# je_io_gamepad_button_down_get

## 函数签名

```cpp
JE_API bool je_io_gamepad_button_down_get(
    je_io_gamepad_handle_t gamepad, jeecs::input::gamepadcode code);
```

## 描述

获取指定虚拟手柄的指定按键是否被按下。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| gamepad | je_io_gamepad_handle_t | 虚拟手柄句柄 |
| code | jeecs::input::gamepadcode | 手柄按键枚举值 |

## 返回值

| 类型 | 描述 |
|------|------|
| bool | true 表示按键被按下，false 表示按键未按下 |

## 用法示例

```cpp
// 检查 A 按钮是否被按下
if (je_io_gamepad_button_down_get(gamepad, jeecs::input::gamepadcode::A)) {
    // A 按钮被按下
}

// 检查开始按钮
if (je_io_gamepad_button_down_get(gamepad, jeecs::input::gamepadcode::START)) {
    // 暂停游戏
}
```

## 注意事项

- 若指定的按键不存在，则始终返回 false
- 如果手柄已经被断开（`je_io_gamepad_close`），调用此接口依然是合法的，但返回值始终是 false

## 相关接口

- [je_io_gamepad_button_state_update](je_io_gamepad_button_state_update.md) - 更新手柄按键状态
- [je_io_gamepad_close](je_io_gamepad_close.md) - 关闭虚拟手柄实例
