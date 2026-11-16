# je_audio_source_pause

## 函数签名

```cpp
JE_API void je_audio_source_pause(je_audio_source_t* source);
```

## 描述

暂停音频源的播放。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | je_audio_source_t* | 音频源指针 |

## 返回值

无

## 用法示例

```cpp
// 暂停播放
je_audio_source_pause(source);

// 继续播放
je_audio_source_play(source);
```

## 注意事项

- 暂停后调用 `je_audio_source_play` 会从暂停位置继续播放
- 暂停的声源会归还硬件声源，继续播放时重新占用；即使同时存在大量暂停的
  声源，也不会占用播放设备的声源配额

## 相关接口

- [je_audio_source_play](je_audio_source_play.md) - 播放/继续播放
- [je_audio_source_stop](je_audio_source_stop.md) - 停止播放
