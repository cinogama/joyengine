# jeecs_fimg_writer_pack_file

## 函数签名

```c
JE_API bool jeecs_fimg_writer_pack_file(
    jeecs_fimg_writer* writer,
    const char* filepath,
    const char* archive_path);
```

## 描述

将一个源文件写入正在创建的镜像，镜像内的路径由 `archive_path` 指定。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `writer` | `jeecs_fimg_writer*` | writer 句柄 |
| `filepath` | `const char*` | 源文件路径 |
| `archive_path` | `const char*` | 镜像内的键，通常形如 `@/...` 或 `!/...` |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 成功返回 `true`；源文件读取失败、路径为空或重复、writer 已结束/失败时返回 `false` |

## 注意事项

- 源文件通过引擎的原生文件操作读取，支持平台自定义实现
- 文件是否压缩由 writer 的压缩选项和归档路径的扩展名决定；PNG 等已压缩格式默认原样存储
- 键重复时保留第一次写入，并记录警告
- 写入过程中出错（例如磁盘已满）会让 writer 进入失败状态

## 相关接口

- [jeecs_fimg_writer_pack_buffer](jeecs_fimg_writer_pack_buffer.md)
- [jeecs_fimg_writer_finish](jeecs_fimg_writer_finish.md)
