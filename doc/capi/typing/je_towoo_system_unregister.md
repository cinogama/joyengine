# je_towoo_system_unregister

## 函数签名

```cpp
JE_API void je_towoo_system_unregister(const jeecs::typing::type_info* tinfo);
```

## 描述

注销一个之前通过 `je_towoo_system_register` 注册的脚本系统。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| tinfo | const jeecs::typing::type_info* | 要注销的系统类型信息 |

## 返回值

无

## 用法示例

```cpp
const jeecs::typing::type_info* sys_type = je_towoo_system_register(...);

// 使用系统...

// 注销系统
je_towoo_system_unregister(sys_type);
```

## 相关接口

- [je_towoo_system_register](je_towoo_system_register.md) - 注册脚本系统
- [je_towoo_api_update](je_towoo_api_update.md) - 更新脚本 API
