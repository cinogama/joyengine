# je_io_close_gamepad

## 函数签名

```cpp
JE_API void je_io_close_gamepad(je_io_gamepad_handle_t gamepad);
```

## 描述

关闭一个虚拟手柄实例。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| gamepad | je_io_gamepad_handle_t | 要关闭的虚拟手柄句柄 |

## 返回值

无

## 用法示例

```cpp
je_io_gamepad_handle_t gamepad = je_io_create_gamepad("Controller", nullptr);
// ... 使用手柄 ...
je_io_close_gamepad(gamepad);
```

## 注意事项

- 关闭后，`je_io_gamepad_is_active` 将返回 false
- 已关闭的手柄不允许执行 `je_io_gamepad_update_button_state` 和 `je_io_gamepad_update_stick` 操作
- 对已关闭的手柄调用 `je_io_gamepad_get_button_down` 和 `je_io_gamepad_get_stick` 是合法的，但返回值固定为默认值

## 相关接口

- [je_io_create_gamepad](je_io_create_gamepad.md) - 创建虚拟手柄实例
- [je_io_gamepad_is_active](je_io_gamepad_is_active.md) - 检查手柄是否活动
