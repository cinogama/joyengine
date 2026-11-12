# je_audio_shared_buffer_mark_outdated

## 函数签名

```cpp
JE_API void je_audio_shared_buffer_mark_outdated(const char* path);
```

## 描述

把 path 从音频缓冲区的内存共享缓存中移除：后续
[je_audio_buffer_load](je_audio_buffer_load.md) 会重新读取（必要时重新生成）
`<音频路径>.je4cache`。已分发的旧缓冲区句柄不受影响（由引用计数保持存活）。

用于编辑器修改 `<音频路径>.je4audio` 之后使新配置生效；磁盘缓存按 CRC64
校验码自动重建，无需手动删除。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| path | const char* | 音频文件的引擎路径（`@/` 或 `!/`） |

## 相关接口

- [je_audio_buffer_load](je_audio_buffer_load.md) - 加载音频
- [je_audio_cache_open_or_create](je_audio_cache_open_or_create.md) - 打开/生成音频缓存
