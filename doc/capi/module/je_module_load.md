# je_module_load

## 函数签名

```cpp
JE_API wo_dylib_handle_t je_module_load(const char* name, const char* path);
```

## 描述

以指定名字加载指定路径的动态库（遵循 Woolang 规则）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| name | const char* | 模块名称 |
| path | const char* | 动态库文件路径 |

## 返回值

| 类型 | 描述 |
|------|------|
| wo_dylib_handle_t | 动态库句柄，加载失败返回 nullptr |

## 用法示例

```cpp
// 加载动态库
wo_dylib_handle_t lib = je_module_load("my_plugin", "@/plugins/my_plugin.dll");

if (lib != nullptr) {
    // 获取函数
    typedef void (*InitFunc)();
    InitFunc init = (InitFunc)je_module_func(lib, "plugin_init");
    
    if (init != nullptr) {
        init();
    }
    
    // 卸载
    je_module_unload(lib);
}
```

## 注意事项

- 路径遵循 Woolang 文件系统规则，支持 `@/` 前缀
- 加载失败时返回 nullptr

## 相关接口

- [je_module_func](je_module_func.md) - 获取模块函数
- [je_module_unload](je_module_unload.md) - 卸载模块
