# je_audio_source_play_process_get

## 函数签名

```cpp
JE_API size_t je_audio_source_play_process_get(je_audio_source_t* source);
```

## 描述

获取当前音频源的播放位置，单位是字节。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | je_audio_source_t* | 音频源指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| size_t | 当前播放位置（字节偏移） |

## 用法示例

```cpp
size_t offset = je_audio_source_play_process_get(source);
printf("当前播放位置: %zu 字节\n", offset);
```

## 相关接口

- [je_audio_source_play_process_set](je_audio_source_play_process_set.md) - 设置播放位置
