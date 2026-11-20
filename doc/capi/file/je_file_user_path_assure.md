# je_file_user_path_assure

## 函数签名

```c
JE_API bool je_file_user_path_assure();
```

## 描述

创建用户数据路径及其缺失的上级目录（等价于 `mkdir -p`）。

`je_file_user_path_set` 只记录路径，不会创建目录；确定最终路径后调用此接口创建。路径已存在时直接成功。底层使用 `std::filesystem::create_directories`，不抛出异常。

## 参数

无参数。

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 目录存在或创建成功返回 `true`；路径为空或创建失败返回 `false` |

创建失败时记录警告。

## 用法

桌面驱动的入口点在进入主循环前自动调用（`driver/main.cpp`）。特殊平台（Android/WebGL）由端口在最终调用 `je_file_user_path_set` 之后自行调用。

### 示例

```cpp
// 设置最终路径后确保目录存在
je_file_user_path_set("/custom/user/data");
if (!je_file_user_path_assure())
    logwarn("用户数据目录不可用");
```

## 注意事项

- 必须在 `je_file_user_path_set` 之后调用；路径为空时返回 `false`
- 可以重复调用；已存在的目录不会被视为错误

## 相关接口

- [je_file_user_path_set](je_file_user_path_set.md) - 设置用户数据路径
- [je_file_user_path_get](je_file_user_path_get.md) - 获取用户数据路径
