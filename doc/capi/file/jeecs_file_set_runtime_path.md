# jeecs_file_set_runtime_path

## 函数签名

```c
JE_API void jeecs_file_set_runtime_path(const char* path);
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
jeecs_file_set_runtime_path("/projects/my_game");

// 现在 "@/assets/texture.png" 将从 "/projects/my_game/assets/texture.png" 加载
```

## 注意事项

- 设置路径时，引擎会尝试以相同参数调用 `jeecs_file_update_default_fimg` 更新默认镜像
- 默认值为可执行文件所在路径

## 相关接口

- [jeecs_file_get_runtime_path](jeecs_file_get_runtime_path.md) - 获取运行时路径
- [jeecs_file_set_host_path](jeecs_file_set_host_path.md) - 设置宿主环境路径
- [jeecs_file_update_default_fimg](jeecs_file_update_default_fimg.md) - 更新默认镜像
