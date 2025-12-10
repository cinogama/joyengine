# je_io_update_mouse_state

## 函数签名

```cpp
JE_API void je_io_update_mouse_state(size_t group, jeecs::input::mousecode key, bool keydown);
```

## 描述

更新鼠标（或触摸点）的按键状态。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| group | size_t | 鼠标/触摸点的分组索引，用于支持多点触控 |
| key | jeecs::input::mousecode | 鼠标按键枚举值 |
| keydown | bool | true 表示按下，false 表示释放 |

## 返回值

无

## 用法示例

```cpp
// 更新主鼠标左键为按下状态
je_io_update_mouse_state(0, jeecs::input::mousecode::LEFT, true);

// 更新主鼠标左键为释放状态
je_io_update_mouse_state(0, jeecs::input::mousecode::LEFT, false);
```

## 注意事项

- 此函数是基本接口，用于由图形后端或平台层向引擎报告鼠标按键状态
- group 参数支持多点触控场景

## 相关接口

- [je_io_get_mouse_state](je_io_get_mouse_state.md) - 获取鼠标的按键状态
- [je_io_update_mouse_pos](je_io_update_mouse_pos.md) - 更新鼠标位置
