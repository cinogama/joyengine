# jeal_play_source

## 函数签名

```cpp
JE_API void jeal_play_source(jeal_source* source);
```

## 描述

播放音频源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | jeal_source* | 音频源指针 |

## 返回值

无

## 用法示例

```cpp
jeal_source* source = jeal_create_source();
const jeal_buffer* buffer = jeal_load_buffer_wav("@/sounds/effect.wav");

jeal_set_source_buffer(source, buffer);
jeal_play_source(source);
```

## 注意事项

- 如果音频源已经在播放或者音频源没有设置音频，则不会有任何效果
- 如果音频源在此前被暂停，则继续播放
- 如果音频源被停止，则从音频的起始位置开始播放

## 相关接口

- [jeal_pause_source](jeal_pause_source.md) - 暂停播放
- [jeal_stop_source](jeal_stop_source.md) - 停止播放
- [jeal_get_source_play_state](jeal_get_source_play_state.md) - 获取播放状态
