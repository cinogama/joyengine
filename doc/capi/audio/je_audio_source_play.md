# je_audio_source_play

## 函数签名

```cpp
JE_API void je_audio_source_play(je_audio_source_t* source);
```

## 描述

播放音频源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | je_audio_source_t* | 音频源指针 |

## 返回值

无

## 用法示例

```cpp
je_audio_source_t* source = je_audio_source_create();
const je_audio_buffer_t* buffer = je_audio_buffer_load("@/sounds/effect.wav");

je_audio_source_buffer_set(source, buffer);
je_audio_source_play(source);
```

## 注意事项

- 如果音频源已经在播放或者音频源没有设置音频，则不会有任何效果
- 如果音频源在此前被暂停，则继续播放
- 如果音频源被停止，则从音频的起始位置开始播放

## 相关接口

- [je_audio_source_pause](je_audio_source_pause.md) - 暂停播放
- [je_audio_source_stop](je_audio_source_stop.md) - 停止播放
- [je_audio_source_play_state_get](je_audio_source_play_state_get.md) - 获取播放状态
