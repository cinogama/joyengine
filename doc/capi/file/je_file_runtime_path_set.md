# je_file_runtime_path_set

## 函数签名

```c
JE_API void je_file_runtime_path_set(const char* path);
```

## 描述

设置当前引擎的运行时路径，不影响"工作路径"。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | 运行时路径 |

## 返回值

无返回值。

## 用法

设置此路径将影响以 `@` 开头的路径的实际位置。

### 示例

```cpp
// 设置运行时路径为项目目录
je_file_runtime_path_set("/projects/my_game");

// 现在 "@/assets/texture.png" 将从 "/projects/my_game/assets/texture.png" 加载
```

## 注意事项

- 此接口不会自动加载镜像；需要另行调用 `je_fimg_default_archive_set`
- 默认值为可执行文件所在路径

## 相关接口

- [je_file_runtime_path_get](je_file_runtime_path_get.md) - 获取运行时路径
- [je_file_host_path_set](je_file_host_path_set.md) - 设置宿主环境路径
- [je_fimg_default_archive_set](je_fimg_default_archive_set.md) - 加载默认镜像
