# jeecs_fimg_archive_open

## 函数签名

```c
JE_API jeecs_fimg_archive* jeecs_fimg_archive_open(const char* root);
```

## 描述

从指定目录打开一个 fimg 镜像，用于查询镜像内容。目录中应包含 `fimg.je4index` 与 `fimg.<N>.je4image`。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `root` | `const char*` | 镜像目录 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jeecs_fimg_archive*` | 镜像句柄；目录不存在清单或清单损坏时返回 `nullptr` |

## 用法

```cpp
jeecs_fimg_archive* archive = jeecs_fimg_archive_open("/srv/game");
if (archive != nullptr) {
    if (jeecs_fimg_archive_has_file(archive, "@/resource/worlds/main.je4world")) {
        size_t size = jeecs_fimg_archive_file_size(archive, "@/resource/worlds/main.je4world");
    }
    jeecs_fimg_archive_close(archive);
}
```

## 注意事项

- 使用完毕后必须调用 `jeecs_fimg_archive_close`
- 普通运行时读取不需要手动打开镜像，`jeecs_file_open` 会使用默认镜像

## 相关接口

- [jeecs_fimg_archive_close](jeecs_fimg_archive_close.md)
- [jeecs_fimg_archive_has_file](jeecs_fimg_archive_has_file.md)
- [jeecs_fimg_archive_file_size](jeecs_fimg_archive_file_size.md)
