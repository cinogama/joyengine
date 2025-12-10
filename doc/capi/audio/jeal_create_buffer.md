# jeal_create_buffer

## 函数签名

```cpp
JE_API const jeal_buffer* jeal_create_buffer(
    const void* data,
    size_t buffer_data_len,
    size_t sample_rate,
    jeal_format format);
```

## 描述

创建一个音频缓冲区。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| data | const void* | 音频数据的指针 |
| buffer_data_len | size_t | 音频数据的长度（字节数） |
| sample_rate | size_t | 采样率（Hz） |
| format | jeal_format | 音频格式枚举值 |

### jeal_format 枚举值

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
| const jeal_buffer* | 指向音频缓冲区的指针 |

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

const jeal_buffer* buffer = jeal_create_buffer(
    data,
    sample_count * sizeof(int16_t),
    sample_rate,
    JE_AUDIO_FORMAT_MONO16
);

delete[] data;

// 使用缓冲区...

jeal_close_buffer(buffer);
```

## 注意事项

- 使用完毕后需要调用 `jeal_close_buffer` 释放资源
- 创建后数据会被复制，可以安全地释放原始数据

## 相关接口

- [jeal_close_buffer](jeal_close_buffer.md) - 释放音频缓冲区
- [jeal_load_buffer_wav](jeal_load_buffer_wav.md) - 从 WAV 文件加载缓冲区
