# je_audio_buffer_close

## 函数签名

```cpp
JE_API void je_audio_buffer_close(const je_audio_buffer_t* buffer);
```

## 描述

释放调用方持有的一个音频缓冲区引用。

缓冲区按引用计数管理：`je_audio_buffer_load` 的每次调用（以及每个持有缓冲
区的声源）各持有一份引用，此处只释放调用方的一份；最后一份引用释放时缓冲
区才真正销毁，流式缓冲区一并关闭其保持打开的 `.je4cache` 文件句柄。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| buffer | const je_audio_buffer_t* | 要释放的音频缓冲区指针 |

## 返回值

无

## 用法示例

```cpp
const je_audio_buffer_t* buffer = je_audio_buffer_load("@/sounds/effect.wav");

// 使用缓冲区...

je_audio_buffer_close(buffer);
```

## 注意事项

- 释放调用方的引用前无需确保没有音频源正在使用该缓冲区：声源持有自己的
  引用，关闭仍被绑定的缓冲区是安全的
- 最后一份引用释放时，共享缓存条目随之移除；再次加载会重新解码
- 流式缓冲区在最后一份引用释放时一并关闭其保持打开的 `.je4cache` 文件句柄

## 相关接口

- [je_audio_buffer_create](je_audio_buffer_create.md) - 创建音频缓冲区
- [je_audio_buffer_load](je_audio_buffer_load.md) - 从音频文件加载缓冲区（缓存管线）
