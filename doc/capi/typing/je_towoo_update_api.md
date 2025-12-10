# je_towoo_update_api

## 函数签名

```cpp
JE_API void je_towoo_update_api();
```

## 描述

更新 Woolang 脚本系统的 API 绑定。当引擎的类型系统发生变化时，需要调用此函数以同步脚本系统的 API。

## 参数

无

## 返回值

无

## 用法示例

```cpp
// 注册新的组件类型后更新脚本 API
je_typing_register(...);
je_towoo_update_api();
```

## 注意事项

- 在动态注册或注销类型后应调用此函数
- 确保在脚本使用新类型之前调用

## 相关接口

- [je_towoo_register_system](je_towoo_register_system.md) - 注册脚本系统
- [je_towoo_unregister_system](je_towoo_unregister_system.md) - 注销脚本系统
