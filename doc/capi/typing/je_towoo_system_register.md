# je_towoo_system_register

## 函数签名

```cpp
JE_API const jeecs::typing::type_info* je_towoo_system_register(
    const char* name, void(*init)(void*), void(*update)(void*), void(*shutdown)(void*));
```

## 描述

注册一个 Woolang 脚本系统到引擎的类型系统中。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| name | const char* | 系统名称 |
| init | void(*)(void*) | 系统初始化函数 |
| update | void(*)(void*) | 系统更新函数 |
| shutdown | void(*)(void*) | 系统关闭函数 |

## 返回值

| 类型 | 描述 |
|------|------|
| const jeecs::typing::type_info* | 注册的系统类型信息 |

## 用法示例

```cpp
void my_system_init(void* sys) {
    // 初始化系统
}

void my_system_update(void* sys) {
    // 更新系统
}

void my_system_shutdown(void* sys) {
    // 关闭系统
}

const jeecs::typing::type_info* sys_type = je_towoo_system_register(
    "MyScriptSystem",
    my_system_init,
    my_system_update,
    my_system_shutdown
);
```

## 相关接口

- [je_towoo_system_unregister](je_towoo_system_unregister.md) - 注销脚本系统
- [je_towoo_api_update](je_towoo_api_update.md) - 更新脚本 API
