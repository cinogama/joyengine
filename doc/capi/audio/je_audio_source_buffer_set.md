# je_audio_source_buffer_set

## 函数签名

```cpp
JE_API bool je_audio_source_buffer_set(jeal_source* source, const jeal_buffer* buffer);
```

## 描述

设置音频源的播放音频。只有音源设置有音频时，其他的播放等动作才会生效。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | jeal_source* | 音频源指针 |
| buffer | const jeal_buffer* | 音频缓冲区指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| bool | 如果设置的音频与先前的音频不同，自动停止播放并返回 true |

## 用法示例

```cpp
jeal_source* source = je_audio_source_create();
const jeal_buffer* buffer = je_audio_buffer_load_wav("@/sounds/bgm.wav");

// 设置音频缓冲区
bool changed = je_audio_source_buffer_set(source, buffer);

// 播放
je_audio_source_play(source);
```

## 注意事项

- 如果设置的音频与先前的音频不同，会自动停止当前播放
- 必须先设置缓冲区才能进行播放操作

## 相关接口

- [je_audio_source_create](je_audio_source_create.md) - 创建音频源
- [je_audio_source_play](je_audio_source_play.md) - 播放音频源
- [je_audio_buffer_create](je_audio_buffer_create.md) - 创建音频缓冲区
