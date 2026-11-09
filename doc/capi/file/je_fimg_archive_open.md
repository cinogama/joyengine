# je_fimg_archive_open

## 函数签名

```c
JE_API je_fimg_archive_t* je_fimg_archive_open(const char* root);
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
| `je_fimg_archive_t*` | 镜像句柄；目录不存在清单或清单损坏时返回 `nullptr` |

## 用法

```cpp
je_fimg_archive_t* archive = je_fimg_archive_open("/srv/game");
if (archive != nullptr) {
    if (je_fimg_archive_has_file(archive, "@/resource/worlds/main.je4world")) {
        size_t size = je_fimg_archive_file_size(archive, "@/resource/worlds/main.je4world");
    }
    je_fimg_archive_close(archive);
}
```

## 注意事项

- 使用完毕后必须调用 `je_fimg_archive_close`
- 普通运行时读取不需要手动打开镜像，`je_file_open` 会使用默认镜像

## 相关接口

- [je_fimg_archive_close](je_fimg_archive_close.md)
- [je_fimg_archive_has_file](je_fimg_archive_has_file.md)
- [je_fimg_archive_file_size](je_fimg_archive_file_size.md)
