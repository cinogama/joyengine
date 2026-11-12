# je_audio_cache_open_or_create

## 函数签名

```cpp
JE_API je_file_t* je_audio_cache_open_or_create(const char* path);
```

## 描述

打开音频文件的二进制缓存 `<音频路径>.je4cache`：缓存有效时直接返回，
缺失或失效时调用内置生成脚本（`builtin/script/internal/cache/audio.wo`，
经 pkg::audio 解码源文件并合并可选的 `<音频路径>.je4audio` 流式播放配置）
重新生成后再返回。

缓存键为音频文件与 `.je4audio` 属性源文件内容连续计算的 CRC64，任一文件
变化都会使缓存失效。缓存文档结构（`je_cache_binary_*` 格式）：

- `channels` / `samplerate` / `bits` -> INT32
- `is_float` / `streaming` -> BOOL
- `pcm` -> BUFFER（原始交织 PCM，帧数 = len / (channels * bits/8)）

与纹理缓存一致：调用方仅在源文件存在时调用本接口触发生成；导出的运行时
只携带缓存，直接用 `je_cache_file_load` 读取。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| path | const char* | 音频文件的引擎路径（`@/` 或 `!/`） |

## 返回值

| 类型 | 描述 |
|------|------|
| je_file_t* | 缓存文件的只读句柄，用毕 `je_file_close` 关闭；失败返回 nullptr |

## 相关接口

- [je_audio_buffer_load](je_audio_buffer_load.md) - 加载音频（内部走同一缓存管线）
- [je_cache_file_load](../file/je_cache_file_load.md) - 只读打开缓存文件
