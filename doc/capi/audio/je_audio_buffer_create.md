# je_audio_buffer_create

## 函数签名

```cpp
JE_API const je_audio_buffer_t* je_audio_buffer_create(
    const void* data,
    size_t buffer_data_len,
    size_t sample_rate,
    je_audio_format_t format);
```

## 描述

创建一个音频缓冲区。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| data | const void* | 音频数据的指针 |
| buffer_data_len | size_t | 音频数据的长度（字节数） |
| sample_rate | size_t | 采样率（Hz） |
| format | je_audio_format_t | 音频格式枚举值 |

### je_audio_format_t 枚举值

| 值 | 描述 |
|----|------|
| JE_AUDIO_FORMAT_MONO8 | 单声道 8 位 |
| JE_AUDIO_FORMAT_MONO16 | 单声道 16 位 |
| JE_AUDIO_FORMAT_MONO32F | 单声道 32 位浮点 |
| JE_AUDIO_FORMAT_STEREO8 | 立体声 8 位 |
| JE_AUDIO_FORMAT_STEREO16 | 立体声 16 位 |
| JE_AUDIO_FORMAT_STEREO32F | 立体声 32 位浮点 |

## 返回值

| 类型 | 描述 |
|------|------|
| const je_audio_buffer_t* | 指向音频缓冲区的指针 |

## 用法示例

```cpp
// 创建一个简单的正弦波音频缓冲区
const size_t sample_rate = 44100;
const size_t duration_ms = 1000;
const size_t sample_count = sample_rate * duration_ms / 1000;
int16_t* data = new int16_t[sample_count];

for (size_t i = 0; i < sample_count; ++i) {
    data[i] = (int16_t)(32767 * sin(2 * 3.14159 * 440 * i / sample_rate));
}

const je_audio_buffer_t* buffer = je_audio_buffer_create(
    data,
    sample_count * sizeof(int16_t),
    sample_rate,
    JE_AUDIO_FORMAT_MONO16
);

delete[] data;

// 使用缓冲区...

je_audio_buffer_close(buffer);
```

## 注意事项

- 使用完毕后需要调用 `je_audio_buffer_close` 释放资源
- 创建后数据会被复制，可以安全地释放原始数据

## 相关接口

- [je_audio_buffer_close](je_audio_buffer_close.md) - 释放音频缓冲区
- [je_audio_buffer_load_wav](je_audio_buffer_load_wav.md) - 从 WAV 文件加载缓冲区
