# je_audio_buffer_load

## 函数签名

```cpp
JE_API const je_audio_buffer_t* je_audio_buffer_load(const char* path);
```

## 描述

加载一个音频文件（当前支持 WAV）。加载走纹理同款的缓存管线：优先读取
`<音频路径>.je4cache` 缓存（裸 PCM 区 + 解码信息文档），缓存缺失或失效时
由内置生成脚本（pkg::audio 解码 + 可选的 `<音频路径>.je4audio` 流式播放
与缓存格式配置）重新生成；导出的运行时只携带缓存，此时源文件不存在也不会
触发生成。

相同路径的重复加载会各自创建独立的缓冲区（音频数据不共享）。

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

- 使用完毕后需要调用 `je_audio_buffer_close` 释放资源
- 如果加载失败，返回值为 nullptr
- 缓冲区的 `m_streaming` 字段来自可选的 `<音频路径>.je4audio` 配置
- 缓存生成时 PCM 一律转换为 `<音频路径>.je4audio` 的 `format`
  （"i8" 缺省 / "i16" / "f32"），任何源编码（含 24 位、整数 32 位、
  64 位浮点）都可加载；仅单声道/立体声受支持（生成期校验）

## 相关接口

- [je_audio_buffer_close](je_audio_buffer_close.md) - 释放音频缓冲区
- [je_audio_buffer_create](je_audio_buffer_create.md) - 从原始数据创建缓冲区
- [je_audio_cache_open_or_create](je_audio_cache_open_or_create.md) - 打开/生成音频缓存
