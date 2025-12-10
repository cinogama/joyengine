# je_io_gamepad_get

## 函数签名

```cpp
JE_API size_t je_io_gamepad_get(size_t count, je_io_gamepad_handle_t* out_gamepads);
```

## 描述

获取所有虚拟手柄的句柄。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| count | size_t | 指示传入的 out_gamepads 可容纳的最大数量 |
| out_gamepads | je_io_gamepad_handle_t* | 输出数组，接收手柄句柄 |

## 返回值

| 类型 | 描述 |
|------|------|
| size_t | 实际返回的手柄数量 |

## 用法示例

```cpp
// 先获取手柄数量
size_t count = je_io_gamepad_get(0, nullptr);

// 分配数组并获取所有手柄句柄
je_io_gamepad_handle_t* gamepads = new je_io_gamepad_handle_t[count];
size_t actual = je_io_gamepad_get(count, gamepads);

for (size_t i = 0; i < actual; ++i) {
    const char* name = je_io_gamepad_name(gamepads[i]);
    // ... 处理手柄 ...
}

delete[] gamepads;
```

## 注意事项

- 如果实际手柄数量大于 count，则只返回前 count 个句柄
- 如果实际手柄数量小于 count，则返回实际数量
- 作为特例，当 count 为 0 时，out_gamepads 可以为 nullptr，此时函数仅返回实际手柄数量

## 相关接口

- [je_io_create_gamepad](je_io_create_gamepad.md) - 创建虚拟手柄实例
- [je_io_close_gamepad](je_io_close_gamepad.md) - 关闭虚拟手柄实例
