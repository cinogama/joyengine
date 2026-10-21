# jeecs_fimg_archive_file_size

## 函数签名

```c
JE_API size_t jeecs_fimg_archive_file_size(const jeecs_fimg_archive* archive, const char* path);
```

## 描述

获取镜像中指定文件的大小（解压后的逻辑大小）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `archive` | `const jeecs_fimg_archive*` | 镜像句柄 |
| `path` | `const char*` | 镜像内的键 |

## 返回值

| 类型 | 描述 |
|------|------|
| `size_t` | 文件大小；文件不存在或参数非法时返回 0 |

## 相关接口

- [jeecs_fimg_archive_has_file](jeecs_fimg_archive_has_file.md)
