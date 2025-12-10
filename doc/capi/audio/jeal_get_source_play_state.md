# jeal_get_source_play_state

## 函数签名

```cpp
JE_API jeal_state jeal_get_source_play_state(jeal_source* source);
```

## 描述

获取当前音频源的状态（是否播放、暂停或停止）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | jeal_source* | 音频源指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| jeal_state | 音频源的播放状态 |

### jeal_state 枚举值

| 值 | 描述 |
|----|------|
| JE_AUDIO_STATE_STOPPED | 已停止 |
| JE_AUDIO_STATE_PLAYING | 正在播放 |
| JE_AUDIO_STATE_PAUSED | 已暂停 |

## 用法示例

```cpp
jeal_state state = jeal_get_source_play_state(source);

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

- [jeal_play_source](jeal_play_source.md) - 播放音频源
- [jeal_pause_source](jeal_pause_source.md) - 暂停播放
- [jeal_stop_source](jeal_stop_source.md) - 停止播放
