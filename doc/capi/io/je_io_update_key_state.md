# je_io_update_key_state

## 函数签名

```c
JE_API void je_io_update_key_state(jeecs::input::keycode keycode, bool keydown);
```

## 描述

更新指定键的状态信息。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `keycode` | `jeecs::input::keycode` | 键码 |
| `keydown` | `bool` | 是否按下 |

## 返回值

无返回值。

## 用法

此函数用于更新键盘按键状态，通常由图形后端在接收到输入事件时调用。

### 示例

```cpp
// 按下 A 键
je_io_update_key_state(jeecs::input::keycode::A, true);

// 释放 A 键
je_io_update_key_state(jeecs::input::keycode::A, false);
```

## 注意事项

- 此函数仅更新内部状态，不会产生系统输入事件

## 相关接口

- [je_io_get_key_down](je_io_get_key_down.md) - 获取按键状态
