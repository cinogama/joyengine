# jeecs_file_update_default_fimg

## 函数签名

```c
JE_API bool jeecs_file_update_default_fimg(const char* path);
```

## 描述

设置默认镜像文件 `.fimg`，镜像文件中的文件将作为运行时 `@` 路径的替补。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | 可能包含镜像文件的目录路径 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 成功返回 `true`，失败返回 `false` |

## 用法

此函数用于加载指定目录下的 `.fimg` 镜像文件，镜像中的文件可作为资源文件的后备来源。

### 示例

```cpp
bool result = jeecs_file_update_default_fimg("/projects/my_game");
if (result) {
    printf("Default fimg updated successfully\n");
}
```

## 注意事项

- `.fimg` 是引擎使用的镜像文件格式
- 当使用 `jeecs_file_open` 打开 `@` 路径下的文件时，如果实际文件不存在，会尝试从镜像中读取
- `jeecs_file_set_runtime_path` 会自动调用此函数

## 相关接口

- [jeecs_file_set_runtime_path](jeecs_file_set_runtime_path.md) - 设置运行时路径
