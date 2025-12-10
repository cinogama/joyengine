# jeal_load_buffer_wav

## 函数签名

```cpp
JE_API const jeal_buffer* jeal_load_buffer_wav(const char* path);
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
| const jeal_buffer* | 指向音频缓冲区的指针，如果加载失败则返回 nullptr |

## 用法示例

```cpp
// 加载 WAV 文件
const jeal_buffer* buffer = jeal_load_buffer_wav("@/sounds/effect.wav");

if (buffer != nullptr) {
    // 创建音频源并设置缓冲区
    jeal_source* source = jeal_create_source();
    jeal_set_source_buffer(source, buffer);
    
    // 播放
    jeal_play_source(source);
    
    // 清理
    jeal_close_source(source);
    jeal_close_buffer(buffer);
}
```

## 注意事项

- 使用完毕后需要调用 `jeal_close_buffer` 释放资源
- 如果加载失败，返回值为 nullptr
- 支持标准 WAV 格式文件

## 相关接口

- [jeal_close_buffer](jeal_close_buffer.md) - 释放音频缓冲区
- [jeal_create_buffer](jeal_create_buffer.md) - 从原始数据创建缓冲区
