# jeecs_fimg_writer_create

## 函数签名

```c
JE_API jeecs_fimg_writer* jeecs_fimg_writer_create(
    const char* root,
    size_t shard_size);
```

## 描述

开始创建 fimg 镜像，镜像文件将被写入 `root` 指定的目录：

- 清单：`fimg.je4index`
- 分片：`fimg.<N>.je4image`

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `root` | `const char*` | 输出目录，必须已经存在且可写 |
| `shard_size` | `size_t` | 单个分片的目标大小；传 0 使用默认值 1 MiB |

## 返回值

| 类型 | 描述 |
|------|------|
| `jeecs_fimg_writer*` | writer 句柄 |

## 用法

```cpp
jeecs_fimg_writer* writer = jeecs_fimg_writer_create("/output/.export", 16 * 1024 * 1024);
jeecs_fimg_writer_pack_file(writer, "/source/texture.png", "@/texture.png");
jeecs_fimg_writer_pack_buffer(writer, data, size, "@/config.json");

if (!jeecs_fimg_writer_finish(writer)) {
    // 生成失败
}
jeecs_fimg_writer_abort(writer);
```

## 注意事项

- 目录由调用方创建；writer 不会创建或清理目录
- 分片按需惰性创建，不会产生空分片
- 写入失败（例如磁盘已满）会在 `pack_*` 或 `finish` 返回 `false`，并记录错误日志
- 每个 writer 使用完都必须调用 `jeecs_fimg_writer_abort` 释放

## 相关接口

- [jeecs_fimg_writer_pack_file](jeecs_fimg_writer_pack_file.md)
- [jeecs_fimg_writer_pack_buffer](jeecs_fimg_writer_pack_buffer.md)
- [jeecs_fimg_writer_finish](jeecs_fimg_writer_finish.md)
- [jeecs_fimg_writer_abort](jeecs_fimg_writer_abort.md)
