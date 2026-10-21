# jeecs_fimg_writer_finish

## 函数签名

```c
JE_API bool jeecs_fimg_writer_finish(jeecs_fimg_writer* writer);
```

## 描述

结束镜像创建：关闭当前分片并写出清单 `fimg.je4index`。清单直接写入目标文件，不做临时文件替换。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `writer` | `jeecs_fimg_writer*` | writer 句柄 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 成功返回 `true`；写入或关闭失败（例如磁盘已满）返回 `false` |

## 注意事项

- 失败时不会生成可用的镜像，错误会记录到日志；调用方应报告生成失败
- 失败后 writer 进入失败状态，再次 `pack_*` 或 `finish` 都会返回 `false`
- 成功与否都必须调用 `jeecs_fimg_writer_abort` 释放 writer
- 输出目录由调用方负责清理，writer 不会删除已经写出的分片

## 相关接口

- [jeecs_fimg_writer_abort](jeecs_fimg_writer_abort.md)
- [jeecs_fimg_writer_create](jeecs_fimg_writer_create.md)
