# je_fimg_writer_create

## 函数签名

```c
JE_API je_fimg_writer_t* je_fimg_writer_create(
    const char* root,
    size_t shard_size,
    /* OPTIONAL */ const je_fimg_writer_options_t* options);
```

## 描述

开始创建 fimg 镜像，镜像文件将被写入 `root` 指定的目录：

- 清单：`fimg.je4index`
- 分片：`fimg.<N>.je4image`

每个文件会根据压缩选项独立选择原样存储或 raw deflate 压缩。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `root` | `const char*` | 输出目录，必须已经存在且可写 |
| `shard_size` | `size_t` | 单个分片的目标大小；传 0 使用默认值 1 MiB |
| `options` | `const je_fimg_writer_options_t*` | 压缩选项；传 `nullptr` 使用默认配置 |

### je_fimg_writer_options_t

| 字段 | 类型 | 描述 |
|------|------|------|
| `compression_level` | `int` | `-1` 使用默认等级 6；`0` 仅存储；`1-9` 使用 deflate |
| `min_compress_size` | `size_t` | 小于该值的文件不压缩 |
| `skip_extensions` | `const char*` | 逗号/分号/空白分隔的扩展名列表；`nullptr` 使用默认列表，空字符串表示不跳过 |

默认配置：等级 6、最小 64 字节、`je_fimg_default_skip_extensions()` 返回的跳过列表。

## 返回值

| 类型 | 描述 |
|------|------|
| `je_fimg_writer_t*` | writer 句柄 |

## 用法

```cpp
je_fimg_writer_options_t options;
options.compression_level = 6;
options.min_compress_size = 1024;
options.skip_extensions = je_fimg_default_skip_extensions();

je_fimg_writer_t* writer = je_fimg_writer_create(
    "/output/.export",
    16 * 1024 * 1024,
    &options);

je_fimg_writer_pack_file(writer, "/source/texture.png", "@/texture.png");
je_fimg_writer_pack_buffer(writer, data, size, "@/config.json");

if (!je_fimg_writer_finish(writer)) {
    // 生成失败
}
je_fimg_writer_abort(writer);
```

## 注意事项

- 目录由调用方创建；writer 不会创建或清理目录
- 分片按需惰性创建，不会产生空分片
- 写入失败（例如磁盘已满）会在 `pack_*` 或 `finish` 返回 `false`，并记录错误日志
- 每个 writer 使用完都必须调用 `je_fimg_writer_abort` 释放

## 相关接口

- [je_fimg_default_skip_extensions](je_fimg_default_skip_extensions.md)
- [je_fimg_writer_pack_file](je_fimg_writer_pack_file.md)
- [je_fimg_writer_pack_buffer](je_fimg_writer_pack_buffer.md)
- [je_fimg_writer_finish](je_fimg_writer_finish.md)
- [je_fimg_writer_abort](je_fimg_writer_abort.md)
