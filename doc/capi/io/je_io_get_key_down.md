# je_io_get_key_down

## 函数签名

```cpp
JE_API bool je_io_get_key_down(jeecs::input::keycode keycode);
```

## 描述

获取指定的按键是否被按下。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| keycode | jeecs::input::keycode | 键盘按键枚举值 |

## 返回值

| 类型 | 描述 |
|------|------|
| bool | true 表示按键被按下，false 表示按键未按下 |

## 用法示例

```cpp
// 检查空格键是否被按下
if (je_io_get_key_down(jeecs::input::keycode::SPACE)) {
    // 空格键被按下
}

// 检查 W 键是否被按下
if (je_io_get_key_down(jeecs::input::keycode::W)) {
    // 向前移动
}
```

## 注意事项

- 此函数返回按键的当前状态，不是按下事件
- 如果需要检测按键的按下/释放事件，需要自行记录上一帧的状态进行比较

## 相关接口

- [je_io_update_key_state](je_io_update_key_state.md) - 更新按键状态
