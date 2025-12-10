# jeecs_file_get_host_path

## 函数签名

```c
JE_API const char* jeecs_file_get_host_path();
```

## 描述

获取当前引擎的宿主环境路径。

## 参数

无参数。

## 返回值

| 类型 | 描述 |
|------|------|
| `const char*` | 当前宿主环境路径字符串 |

## 用法

此函数用于获取以 `!` 开头的路径对应的实际目录。

### 示例

```cpp
const char* host_path = jeecs_file_get_host_path();
printf("Host path: %s\n", host_path);
```

## 注意事项

- 若没有使用 `jeecs_file_set_host_path` 设置，默认为引擎可执行文件所在路径
- 返回的字符串由引擎管理，不需要释放

## 相关接口

- [jeecs_file_set_host_path](jeecs_file_set_host_path.md) - 设置宿主环境路径
