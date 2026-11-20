# je_file_user_path_get

## 函数签名

```c
JE_API const char* je_file_user_path_get();
```

## 描述

获取当前引擎的用户数据路径，与"宿主环境路径""运行时路径"无关。

## 参数

无参数。

## 返回值

| 类型 | 描述 |
|------|------|
| `const char*` | 用户数据路径 |

## 用法

若没有使用 `je_file_user_path_set` 设置此路径，则此路径默认为引擎的可执行文件所在路径。

### 示例

```cpp
// 获取用户数据路径，读取编辑器布局等用户状态
const char* user_path = je_file_user_path_get();
```

## 注意事项

- 返回的指针指向内部存储，路径被 `je_file_user_path_set` 覆盖后失效
- 引擎初始化时会按操作系统标准位置自动推导此路径（参见 `je_file_user_path_set`）

## 相关接口

- [je_file_user_path_set](je_file_user_path_set.md) - 设置用户数据路径
- [je_file_host_path_get](je_file_host_path_get.md) - 获取宿主环境路径
