# jeecs_register_native_file_operator

## 函数签名

```cpp
JE_API void jeecs_register_native_file_operator(
    je_read_file_open_func_t opener,
    je_read_file_func_t reader,
    je_read_file_tell_func_t teller,
    je_read_file_seek_func_t seeker,
    je_read_file_close_func_t closer);
```

## 描述

设置引擎底层的文件读取接口。用于在特殊平台（如 Android）上替换默认的文件操作实现。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| opener | je_read_file_open_func_t | 文件打开函数 |
| reader | je_read_file_func_t | 文件读取函数 |
| teller | je_read_file_tell_func_t | 获取文件位置函数 |
| seeker | je_read_file_seek_func_t | 文件定位函数 |
| closer | je_read_file_close_func_t | 文件关闭函数 |

## 返回值

无

## 用法示例

```cpp
// Android 平台注册 AAssetManager 文件操作
jeecs_register_native_file_operator(
    android_asset_open,
    android_asset_read,
    android_asset_tell,
    android_asset_seek,
    android_asset_close);
```

## 相关接口

- [jeecs_file_open](jeecs_file_open.md) - 打开文件
- [jeecs_file_read](jeecs_file_read.md) - 读取文件
