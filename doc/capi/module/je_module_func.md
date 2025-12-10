# je_module_func

## 函数签名

```cpp
JE_API void* je_module_func(wo_dylib_handle_t lib, const char* funcname);
```

## 描述

从动态库中加载某个函数，返回函数的地址。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| lib | wo_dylib_handle_t | 动态库句柄 |
| funcname | const char* | 函数名称 |

## 返回值

| 类型 | 描述 |
|------|------|
| void* | 函数地址，如果找不到函数返回 nullptr |

## 用法示例

```cpp
wo_dylib_handle_t lib = je_module_load("plugin", "@/plugins/plugin.dll");

if (lib != nullptr) {
    // 获取函数指针
    typedef int (*GetVersionFunc)();
    GetVersionFunc get_version = (GetVersionFunc)je_module_func(lib, "get_version");
    
    if (get_version != nullptr) {
        int version = get_version();
        printf("Plugin version: %d\n", version);
    }
}
```

## 相关接口

- [je_module_load](je_module_load.md) - 加载模块
- [je_module_unload](je_module_unload.md) - 卸载模块
