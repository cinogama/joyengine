# je_file_host_path_set

## 函数签名

```c
JE_API void je_file_host_path_set(const char* path);
```

## 描述

设置当前引擎的宿主环境路径，不影响"实际可执行文件所在路径"。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | 宿主环境路径 |

## 返回值

无返回值。

## 用法

设置此路径将影响以 `!` 开头的路径的实际位置。

### 示例

```cpp
// 设置宿主路径为自定义目录
je_file_host_path_set("/custom/engine/path");

// 现在 "!/builtin/shader.wo" 将从 "/custom/engine/path/builtin/shader.wo" 加载
```

## 注意事项

- 正常情况下不需要调用此接口
- 引擎初始化时会自动设置为实际二进制文件所在路径
- 对于某些特殊平台（如移动端），引擎不能使用默认路径时，可使用此接口指定路径
- 主要用于需要创建缓存文件或读取镜像等资源文件的场景

## 相关接口

- [je_file_host_path_get](je_file_host_path_get.md) - 获取宿主环境路径
- [je_file_runtime_path_set](je_file_runtime_path_set.md) - 设置运行时路径
