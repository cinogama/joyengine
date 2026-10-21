# jeecs_fimg_archive_close

## 函数签名

```c
JE_API void jeecs_fimg_archive_close(jeecs_fimg_archive* archive);
```

## 描述

关闭一个通过 `jeecs_fimg_archive_open` 打开的镜像句柄。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `archive` | `jeecs_fimg_archive*` | 镜像句柄 |

## 注意事项

- 关闭镜像不会影响已经打开的 `jeecs_file` 句柄，它们持有自己的分片句柄
- 关闭后不要再使用该镜像句柄

## 相关接口

- [jeecs_fimg_archive_open](jeecs_fimg_archive_open.md)
