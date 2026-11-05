# je_audio_buffer_load_wav

## 函数签名

```cpp
JE_API const je_audio_buffer_t* je_audio_buffer_load_wav(const char* path);
```

## 描述

加载一个 WAV 格式的音频文件。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| path | const char* | 音频文件的路径 |

## 返回值

| 类型 | 描述 |
|------|------|
| const je_audio_buffer_t* | 指向音频缓冲区的指针，如果加载失败则返回 nullptr |

## 用法示例

```cpp
// 加载 WAV 文件
const je_audio_buffer_t* buffer = je_audio_buffer_load_wav("@/sounds/effect.wav");

if (buffer != nullptr) {
    // 创建音频源并设置缓冲区
    je_audio_source_t* source = je_audio_source_create();
    je_audio_source_buffer_set(source, buffer);
    
    // 播放
    je_audio_source_play(source);
    
    // 清理
    je_audio_source_close(source);
    je_audio_buffer_close(buffer);
}
```

## 注意事项

- 使用完毕后需要调用 `je_audio_buffer_close` 释放资源
- 如果加载失败，返回值为 nullptr
- 支持标准 WAV 格式文件

## 相关接口

- [je_audio_buffer_close](je_audio_buffer_close.md) - 释放音频缓冲区
- [je_audio_buffer_create](je_audio_buffer_create.md) - 从原始数据创建缓冲区
