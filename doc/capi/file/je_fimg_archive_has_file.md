# je_fimg_archive_has_file

## 函数签名

```c
JE_API bool je_fimg_archive_has_file(const jeecs_fimg_archive* archive, const char* path);
```

## 描述

判断镜像中是否存在指定路径的文件。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `archive` | `const jeecs_fimg_archive*` | 镜像句柄 |
| `path` | `const char*` | 镜像内的键，通常形如 `@/...` 或 `!/...` |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 存在返回 `true` |

## 示例

```cpp
if (je_fimg_archive_has_file(archive, "@/builtin/main.wo")) {
    size_t size = je_fimg_archive_file_size(archive, "@/builtin/main.wo");
}
```

## 相关接口

- [je_fimg_archive_file_size](je_fimg_archive_file_size.md)
- [je_fimg_archive_open](je_fimg_archive_open.md)
