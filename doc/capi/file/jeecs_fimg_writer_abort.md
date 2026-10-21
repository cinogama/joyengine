# jeecs_fimg_writer_abort

## 函数签名

```c
JE_API void jeecs_fimg_writer_abort(jeecs_fimg_writer* writer);
```

## 描述

关闭并释放 writer。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `writer` | `jeecs_fimg_writer*` | writer 句柄 |

## 注意事项

- 如果尚未成功 `finish`，仅关闭当前分片；已经写出的分片保留在磁盘上
- 输出目录由调用方负责清理（例如导出目录会在下次导出时整体重建）
- 每个 `jeecs_fimg_writer_create` 返回的句柄都必须调用此函数释放

## 相关接口

- [jeecs_fimg_writer_finish](jeecs_fimg_writer_finish.md)
