# je_audio_source_stop

## 函数签名

```cpp
JE_API void je_audio_source_stop(je_audio_source_t* source);
```

## 描述

停止音频源的播放。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | je_audio_source_t* | 音频源指针 |

## 返回值

无

## 用法示例

```cpp
// 停止播放
je_audio_source_stop(source);

// 重新播放将从头开始
je_audio_source_play(source);
```

## 注意事项

- 停止后调用 `je_audio_source_play` 会从音频起始位置重新播放

## 相关接口

- [je_audio_source_play](je_audio_source_play.md) - 播放音频源
- [je_audio_source_pause](je_audio_source_pause.md) - 暂停播放
