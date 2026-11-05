# je_audio_buffer_close

## 函数签名

```cpp
JE_API void je_audio_buffer_close(const jeal_buffer* buffer);
```

## 描述

释放一个音频缓冲区。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| buffer | const jeal_buffer* | 要释放的音频缓冲区指针 |

## 返回值

无

## 用法示例

```cpp
const jeal_buffer* buffer = je_audio_buffer_load_wav("@/sounds/effect.wav");

// 使用缓冲区...

je_audio_buffer_close(buffer);
```

## 注意事项

- 释放缓冲区前应确保没有音频源正在使用该缓冲区

## 相关接口

- [je_audio_buffer_create](je_audio_buffer_create.md) - 创建音频缓冲区
- [je_audio_buffer_load_wav](je_audio_buffer_load_wav.md) - 从 WAV 文件加载缓冲区
