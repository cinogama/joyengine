# je_fimg_default_archive_set

## 函数签名

```c
JE_API bool je_fimg_default_archive_set(const char* root);
```

## 描述

从指定目录加载 fimg 镜像作为默认镜像。目录中应包含清单 `fimg.je4index` 和分片 `fimg.<N>.je4image`。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `root` | `const char*` | 镜像目录；传 `nullptr` 表示仅关闭旧镜像 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 加载成功或 `root == nullptr` 时返回 `true`；目录不存在清单或清单损坏返回 `false` |

## 用法

```cpp
je_file_host_path_set("/srv/game");
je_file_runtime_path_set("/srv/game");
je_fimg_default_archive_set("/srv/game");

// 实际文件不存在时，从镜像读取
jeecs_file* f = je_file_open("@/resource/worlds/main.je4world");
```

## 注意事项

- 无论加载是否成功，之前打开的默认镜像都会被替换/关闭
- 已打开的 `jeecs_file` 持有自己的分片句柄，不受镜像替换影响
- 目录下没有 `fimg.je4index` 时静默返回 `false`，这是开发期加载松散文件的正常情况
- `je_file_runtime_path_set` 不再自动加载镜像，需要显式调用此函数

## 相关接口

- [je_fimg_archive_open](je_fimg_archive_open.md) - 打开一个镜像用于查询
- [je_file_open](je_file_open.md) - 打开文件（`@`/`!` 路径会回退到默认镜像）
- [fimg 格式说明](../../fimg_format.md)
