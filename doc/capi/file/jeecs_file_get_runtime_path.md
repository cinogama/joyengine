# jeecs_file_get_runtime_path

## 函数签名

```c
JE_API const char* jeecs_file_get_runtime_path();
```

## 描述

获取当前引擎的运行时路径，与"工作路径"无关。

## 参数

无参数。

## 返回值

| 类型 | 描述 |
|------|------|
| `const char*` | 当前运行时路径字符串 |

## 用法

此函数用于获取以 `@` 开头的路径对应的实际目录。

### 示例

```cpp
const char* runtime_path = jeecs_file_get_runtime_path();
printf("Runtime path: %s\n", runtime_path);
```

## 注意事项

- 若没有使用 `jeecs_file_set_runtime_path` 设置，默认为引擎可执行文件所在路径
- 返回的字符串由引擎管理，不需要释放

## 相关接口

- [jeecs_file_set_runtime_path](jeecs_file_set_runtime_path.md) - 设置运行时路径
