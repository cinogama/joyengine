# je_io_get_mouse_state

## 函数签名

```cpp
JE_API bool je_io_get_mouse_state(size_t group, jeecs::input::mousecode key);
```

## 描述

获取鼠标的按键状态。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| group | size_t | 鼠标/触摸点的分组索引 |
| key | jeecs::input::mousecode | 鼠标按键枚举值 |

## 返回值

| 类型 | 描述 |
|------|------|
| bool | true 表示按键被按下，false 表示按键未按下 |

## 用法示例

```cpp
// 检查主鼠标左键是否被按下
if (je_io_get_mouse_state(0, jeecs::input::mousecode::LEFT)) {
    // 鼠标左键被按下
}
```

## 注意事项

- group 参数支持多点触控场景

## 相关接口

- [je_io_update_mouse_state](je_io_update_mouse_state.md) - 更新鼠标按键状态
