# je_io_create_gamepad

## 函数签名

```cpp
JE_API je_io_gamepad_handle_t je_io_create_gamepad(
    const char* name_may_null, const char* guid_may_null);
```

## 描述

创建一个虚拟手柄实例。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| name_may_null | const char* | 手柄名称，用于以可读形式分辨不同的控制器设备。如果为 nullptr，则分配一个默认名称 |
| guid_may_null | const char* | 手柄 GUID，用于区分不同的物理设备。如果为 nullptr，则由引擎生成 |

## 返回值

| 类型 | 描述 |
|------|------|
| je_io_gamepad_handle_t | 虚拟手柄实例句柄 |

## 用法示例

```cpp
// 创建一个有名称的虚拟手柄
je_io_gamepad_handle_t gamepad = je_io_create_gamepad("Xbox Controller", "xinput-0");

// 创建使用默认名称和自动生成 GUID 的手柄
je_io_gamepad_handle_t gamepad2 = je_io_create_gamepad(nullptr, nullptr);
```

## 注意事项

- 虚拟手柄和输入设备的对应关系通过此函数构建
- 不一定是实际的物理游戏手柄，例如可以将键盘按键映射到虚拟手柄上
- 使用完毕后需调用 `je_io_close_gamepad` 关闭手柄

## 相关接口

- [je_io_close_gamepad](je_io_close_gamepad.md) - 关闭虚拟手柄实例
- [je_io_gamepad_name](je_io_gamepad_name.md) - 获取手柄名称
- [je_io_gamepad_guid](je_io_gamepad_guid.md) - 获取手柄 GUID
