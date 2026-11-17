# je_audio_buffer_load

## 函数签名

```cpp
JE_API const je_audio_buffer_t* je_audio_buffer_load(const char* path);
```

## 描述

加载一个音频文件（当前支持 WAV）。加载走纹理同款的缓存管线：优先读取
`<音频路径>.je4cache` 缓存（编码载荷区 + 解码信息文档），缓存缺失或失效时
由内置生成脚本（pkg::audio 解码 + 可选的 `<音频路径>.je4audio` 流式播放、
压缩编码与质量配置）重新生成；导出的运行时只携带缓存，此时源文件不存在也
不会触发生成。

相同路径、且缓存版本（缓存头的 crc64，随 wav 与 `.je4audio` 内容变化）相同的
重复加载共享同一个缓冲区实例：每次加载持有该实例的一份引用，
`je_audio_buffer_close` 只释放本次加载持有的引用，最后一份引用释放时缓冲区
才真正关闭。路径缓存是弱引用（本身不持有引用计数），不会让音频常驻内存。

缓存重建（如修改音频或 `<音频路径>.je4audio`）后校验码随之改变，新的加载会
解码出新实例，不会继续复用旧数据；旧实例仍由既有引用持有，直到其释放。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| path | const char* | 音频文件的引擎路径（`@/` 或 `!/`） |

## 返回值

| 类型 | 描述 |
|------|------|
| const je_audio_buffer_t* | 指向音频缓冲区的指针，加载失败（文件/缓存不可用或格式不支持）则返回 nullptr |

## 用法示例

```cpp
// 加载音频文件
const je_audio_buffer_t* buffer = je_audio_buffer_load("@/sounds/effect.wav");

if (buffer != nullptr) {
    // 创建音频源并设置缓冲区
    je_audio_source_t* source = je_audio_source_create();
    je_audio_source_buffer_set(source, buffer);

    // 播放
    je_audio_source_play(source);

    // 清理
    je_audio_source_close(source);
    je_audio_buffer_close(buffer);
}
```

## 注意事项

- 使用完毕后需要调用 `je_audio_buffer_close` 释放本次加载持有的引用；仍有
  其它引用（声源或同路径的重复加载）时缓冲区保持存活
- 如果加载失败，返回值为 nullptr
- 缓冲区的 `m_streaming` 字段来自可选的 `<音频路径>.je4audio` 配置
- 缓存生成时 PCM 一律转换为 `<音频路径>.je4audio` 的 `format`
  （"i8" 缺省 / "i16" / "f32"）；`codec` 可选 `"none"`（缺省，无损的
  PCM 档）/ `"adpcm"`（有损，解码为 16 位整数）/ `"vorbis"`（有损，
  解码为 32 位浮点，`quality` 0.0~1.0 可调）。任何源编码（含 24 位、
  整数 32 位、64 位浮点）都可加载；仅单声道/立体声受支持（生成期校验）
- `m_codec` 记录载荷区的压缩编码；`m_data` / `m_size` 始终是解码后的 PCM
- `m_streaming` 为 true 时按流式播放：整段 PCM 不读入内存（`m_data` 为
  nullptr），缓冲区持有并保持打开 `.je4cache`，由声源播放时按约 0.25 秒
  一块解码读取；`m_size` 仍是解码后 PCM 总字节数（用于计算时长），设备
  线程每 100ms 补块，循环、暂停/继续、停止与跳转的语义与整段载入的缓冲区
  一致
- 流式缓冲区在最后一份引用释放时一并关闭其缓存文件；`m_stream_instance` 是
  引擎内部数据源，不要自行关闭其中的文件

## 相关接口

- [je_audio_buffer_close](je_audio_buffer_close.md) - 释放音频缓冲区
- [je_audio_buffer_create](je_audio_buffer_create.md) - 从原始数据创建缓冲区
- [je_audio_cache_open_or_create](je_audio_cache_open_or_create.md) - 打开/生成音频缓存
