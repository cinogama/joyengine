# je_audio_source_play_state_get

## 函数签名

```cpp
JE_API je_audio_state_t je_audio_source_play_state_get(je_audio_source_t* source);
```

## 描述

获取当前音频源的状态（是否播放、暂停或停止）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | je_audio_source_t* | 音频源指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| je_audio_state_t | 音频源的播放状态 |

### je_audio_state_t 枚举值

| 值 | 描述 |
|----|------|
| JE_AUDIO_STATE_STOPPED | 已停止 |
| JE_AUDIO_STATE_PLAYING | 正在播放 |
| JE_AUDIO_STATE_PAUSED | 已暂停 |

## 用法示例

```cpp
je_audio_state_t state = je_audio_source_play_state_get(source);

switch (state) {
    case JE_AUDIO_STATE_STOPPED:
        printf("音频已停止\n");
        break;
    case JE_AUDIO_STATE_PLAYING:
        printf("音频正在播放\n");
        break;
    case JE_AUDIO_STATE_PAUSED:
        printf("音频已暂停\n");
        break;
}
```

## 相关接口

- [je_audio_source_play](je_audio_source_play.md) - 播放音频源
- [je_audio_source_pause](je_audio_source_pause.md) - 暂停播放
- [je_audio_source_stop](je_audio_source_stop.md) - 停止播放
