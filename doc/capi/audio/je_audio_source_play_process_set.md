# je_audio_source_play_process_set

## 函数签名

```cpp
JE_API void je_audio_source_play_process_set(jeal_source* source, size_t offset);
```

## 描述

设置音频源的播放进度，单位是字节。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | jeal_source* | 音频源指针 |
| offset | size_t | 目标播放位置（字节偏移） |

## 返回值

无

## 用法示例

```cpp
// 跳转到指定位置播放
je_audio_source_play_process_set(source, 44100 * 2);  // 对于 16 位单声道，跳转约 1 秒
```

## 注意事项

- 设置的偏移量应当是目标播放音频的采样大小的整数倍
- 如果未对齐，会被强制对齐

## 相关接口

- [je_audio_source_play_process_get](je_audio_source_play_process_get.md) - 获取播放位置
