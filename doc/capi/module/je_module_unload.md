# je_module_unload

## 函数签名

```cpp
JE_API void je_module_unload(wo_dylib_handle_t lib);
```

## 描述

立即卸载某个动态库。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| lib | wo_dylib_handle_t | 要卸载的动态库句柄 |

## 返回值

无

## 用法示例

```cpp
wo_dylib_handle_t lib = je_module_load("plugin", "@/plugins/plugin.dll");

// 使用模块...

je_module_unload(lib);
```

## 注意事项

- 卸载后不应再使用该模块中的任何函数
- 确保在卸载前已释放模块相关资源

## 相关接口

- [je_module_load](je_module_load.md) - 加载模块
- [je_module_func](je_module_func.md) - 获取模块函数
