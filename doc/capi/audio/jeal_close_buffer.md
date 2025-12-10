# jeal_close_buffer

## 函数签名

```cpp
JE_API void jeal_close_buffer(const jeal_buffer* buffer);
```

## 描述

释放一个音频缓冲区。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| buffer | const jeal_buffer* | 要释放的音频缓冲区指针 |

## 返回值

无

## 用法示例

```cpp
const jeal_buffer* buffer = jeal_load_buffer_wav("@/sounds/effect.wav");

// 使用缓冲区...

jeal_close_buffer(buffer);
```

## 注意事项

- 释放缓冲区前应确保没有音频源正在使用该缓冲区

## 相关接口

- [jeal_create_buffer](jeal_create_buffer.md) - 创建音频缓冲区
- [jeal_load_buffer_wav](jeal_load_buffer_wav.md) - 从 WAV 文件加载缓冲区
