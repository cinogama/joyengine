# je_io_gamepad_name

## 函数签名

```cpp
JE_API const char* je_io_gamepad_name(je_io_gamepad_handle_t gamepad);
```

## 描述

获取指定虚拟手柄的名称。名称在创建时指定，用于以可读形式分辨不同的控制器设备。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| gamepad | je_io_gamepad_handle_t | 虚拟手柄句柄 |

## 返回值

| 类型 | 描述 |
|------|------|
| const char* | 手柄名称字符串 |

## 用法示例

```cpp
je_io_gamepad_handle_t gamepad = je_io_create_gamepad("Xbox Controller", nullptr);
const char* name = je_io_gamepad_name(gamepad);
// name = "Xbox Controller"
```

## 相关接口

- [je_io_create_gamepad](je_io_create_gamepad.md) - 创建虚拟手柄实例
- [je_io_gamepad_guid](je_io_gamepad_guid.md) - 获取手柄 GUID
