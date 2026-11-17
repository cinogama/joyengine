# je_audio_cache_open_or_create

## 函数签名

```cpp
JE_API je_file_t* je_audio_cache_open_or_create(const char* path);
```

## 描述

打开音频文件的二进制缓存 `<音频路径>.je4cache`：缓存有效时直接返回，
缺失或失效时调用内置生成脚本（`builtin/script/internal/cache/audio.wo`，
经 pkg::audio 解码源文件并合并可选的 `<音频路径>.je4audio` 配置）重新
生成后再返回。

`.je4audio` 可配置：

- `streaming`（BOOL，缺省 false）—— 该音频应以流式方式播放
- `format`（"i8" 缺省 / "i16" / "f32"）—— PCM 缓存格式；PCM 经
  pkg::audio 转换后存储（声道数与帧数不变，样本值钳制到 [-1, 1]），
  任何源编码（含 24 位、整数 32 位、64 位浮点）都可落成 OpenAL 可用的
  缓存。PCM 档不做压缩，是该位深下的无损档
- `codec`（"none" 缺省 / "adpcm" / "vorbis"）—— 载荷区压缩编码：
  - `adpcm`：IMA-ADPCM 有损压缩（约 4:1），解码为 16 位整数，覆盖
    `format` 的位深
  - `vorbis`：Vorbis 有损压缩（约 8-12:1），解码为 32 位浮点，覆盖
    `format` 的位深
- `quality`（FLOAT，缺省 0.5）—— Vorbis 压缩质量，取值 0.0~1.0
  （越大音质越好、缓存越大）；其它编码忽略

未知的 `format` / `codec` 取值或越界的 `quality` 会中止生成并报错。
Vorbis 的压缩质量必须与编码一起配置：`.je4audio` 参与缓存键，修改质量后
缓存会自动重建。

仅支持单声道/立体声源（OpenAL 缓冲格式约束），生成期校验并报错。

缓存键为音频文件与 `.je4audio` 属性源文件内容连续计算的 CRC64，任一文件
变化都会使缓存失效。缓存布局（文件头之后）：

- `[uint32 codec]` —— 载荷区始终以编码 id 开头（`je_audio_codec_t`）
- `codec == PCM`：`[uint64 pcm_size][pcm 字节]` —— 裸数据区，不套
  `je_cache_binary_*` 值标记，可直接按偏移访问
- `codec == ADPCM`：解码总长/总帧数/块参数 + 块表 + 逐块独立解码的数据
- `codec == VORBIS`：解码总长/总帧数/流长 + 内嵌 Ogg Vorbis 流
- `DOCUMENT` —— 解码信息（`bits` / `is_float` 描述解码后实际存储的 PCM）：
  - `channels` / `samplerate` / `bits` -> INT32
  - `is_float` / `streaming` -> BOOL

`streaming` 为 true 时，声源播放时按约 0.25 秒一块解码读取，整段 PCM
不会载入内存；ADPCM 按块独立解码，Vorbis 用 vorbisfile 精确 seek/循环。

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
- [je_audio_cache_payload_write](je_audio_cache_payload_write.md) - 生成脚本写入编码后的载荷区
- [je_cache_file_load](../file/je_cache_file_load.md) - 只读打开缓存文件
