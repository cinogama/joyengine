# je_io_gamepad_guid

## 函数签名

```cpp
JE_API const char* je_io_gamepad_guid(je_io_gamepad_handle_t gamepad);
```

## 描述

获取指定虚拟手柄的 GUID。GUID 在创建时指定，用于区分不同的物理设备。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| gamepad | je_io_gamepad_handle_t | 虚拟手柄句柄 |

## 返回值

| 类型 | 描述 |
|------|------|
| const char* | 手柄 GUID 字符串 |

## 用法示例

```cpp
je_io_gamepad_handle_t gamepad = je_io_create_gamepad("Controller", "xinput-0");
const char* guid = je_io_gamepad_guid(gamepad);
// guid = "xinput-0"
```

## 相关接口

- [je_io_create_gamepad](je_io_create_gamepad.md) - 创建虚拟手柄实例
- [je_io_gamepad_name](je_io_gamepad_name.md) - 获取手柄名称
