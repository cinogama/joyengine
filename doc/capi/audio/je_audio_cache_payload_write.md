# je_audio_cache_payload_write

## 函数签名

```cpp
JE_API bool je_audio_cache_payload_write(
    je_writing_cache_file_t* file,
    const void* pcm,
    size_t pcm_len,
    int32_t channels,
    int32_t sample_rate,
    int32_t bits,
    bool is_float,
    je_audio_codec_t codec,
    float quality);
```

## 描述

把整段已按目标位深转换的交织 PCM 按 `codec` 编码后写入音频缓存载荷区
（缓存文件头之后、`je_cache_binary_*` 文档之前）。供内置生成脚本
`builtin/script/internal/cache/audio.wo` 调用；一次调用写完整个载荷区，
调用方随后照常追加描述解码信息的文档。

`codec` 与 `(bits, is_float)` 的组合约束：

| codec | 要求 | 解码后的 PCM |
|-------|------|--------------|
| `JE_AUDIO_CODEC_PCM` | 任意 8/16/32 位 PCM | 与输入相同 |
| `JE_AUDIO_CODEC_ADPCM` | 必须 16 位整数 | 16 位整数（约 4:1 有损） |
| `JE_AUDIO_CODEC_VORBIS` | 必须 32 位浮点 | 32 位浮点（约 8-12:1 有损） |

`quality` 只对 Vorbis 生效，取值 `0.0~1.0`：越大音质越好、缓存越大；
其它编码忽略此参数。

载荷区布局（所有多字节整数按本机字节序）：

```
[u32 codec]
PCM    : [u64 pcm_size][pcm bytes]
ADPCM  : [u64 decoded_bytes][u64 total_frames]
         [u32 block_frames][u32 block_count]
         [entries: {u64 offset, u64 start_frame, u32 size, u32 rsv}]
         [blocks: 每声道 4 字节初始状态 + 声道优先的 IMA nibble]
VORBIS : [u64 decoded_bytes][u64 total_frames][u64 ogg_size]
         [内嵌 Ogg Vorbis 流]
```

- ADPCM 块表项的 `offset` 相对载荷起点；每块自带每声道的
  `{i16 predictor, u8 index, u8 rsv}` 初始状态，任意块可独立解码。
- Vorbis 的载荷是一段完整的 Ogg Vorbis 流，整段载入与流式播放都通过
  vorbisfile 解码，流式定位使用 `ov_pcm_seek`。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| file | je_writing_cache_file_t* | `je_cache_file_create` 返回的缓存写入器 |
| pcm | const void* | 按目标位深转换后的交织 PCM 数据 |
| pcm_len | size_t | PCM 字节数，必须是帧大小的整数倍 |
| channels | int32_t | 声道数（仅支持 1 或 2） |
| sample_rate | int32_t | 采样率（Hz） |
| bits | int32_t | 每样本位数（8/16/32） |
| is_float | bool | `true` 表示 32 位 IEEE 浮点，`false` 表示整数 PCM |
| codec | je_audio_codec_t | 载荷区压缩编码 |
| quality | float | Vorbis 压缩质量（0.0~1.0），其它编码忽略 |

## 返回值

| 类型 | 描述 |
|------|------|
| bool | 成功写入返回 true；编码/写入失败返回 false，调用方应中止缓存生成 |

## 相关接口

- [je_audio_cache_open_or_create](je_audio_cache_open_or_create.md) - 打开或生成音频缓存
- [je_cache_file_write](../file/je_cache_file_write.md) - 缓存文件直写
