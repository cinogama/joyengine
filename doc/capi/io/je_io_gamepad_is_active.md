# je_io_gamepad_is_active

## 函数签名

```cpp
JE_API bool je_io_gamepad_is_active(
    je_io_gamepad_handle_t gamepad,
    jeecs::typing::timestamp_ms_t* out_last_pushed_time_may_null);
```

## 描述

检查指定的虚拟手柄，其对应的实际设备是否存在。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| gamepad | je_io_gamepad_handle_t | 虚拟手柄句柄 |
| out_last_pushed_time_may_null | jeecs::typing::timestamp_ms_t* | 输出参数，可选。如果不为 nullptr，则返回手柄的最后操作时间 |

## 返回值

| 类型 | 描述 |
|------|------|
| bool | true 表示手柄活动（设备存在），false 表示手柄已关闭 |

## 用法示例

```cpp
jeecs::typing::timestamp_ms_t lastTime;
if (je_io_gamepad_is_active(gamepad, &lastTime)) {
    // 手柄活动中
    // lastTime 包含最后操作时间
}

// 仅检查是否活动
if (je_io_gamepad_is_active(gamepad, nullptr)) {
    // 手柄活动中
}
```

## 注意事项

- 虚拟手柄和输入设备的对应关系通过 `je_io_create_gamepad` 构建
- 不一定是实际的物理游戏手柄，例如可以将键盘按键映射到虚拟手柄上
- 当 `je_io_close_gamepad` 调用时，虚拟手柄实例会被销毁，此时此函数返回 false

## 相关接口

- [je_io_create_gamepad](je_io_create_gamepad.md) - 创建虚拟手柄实例
- [je_io_close_gamepad](je_io_close_gamepad.md) - 关闭虚拟手柄实例
