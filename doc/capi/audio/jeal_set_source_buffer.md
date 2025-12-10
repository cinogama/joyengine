# jeal_set_source_buffer

## 函数签名

```cpp
JE_API bool jeal_set_source_buffer(jeal_source* source, const jeal_buffer* buffer);
```

## 描述

设置音频源的播放音频。只有音源设置有音频时，其他的播放等动作才会生效。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | jeal_source* | 音频源指针 |
| buffer | const jeal_buffer* | 音频缓冲区指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| bool | 如果设置的音频与先前的音频不同，自动停止播放并返回 true |

## 用法示例

```cpp
jeal_source* source = jeal_create_source();
const jeal_buffer* buffer = jeal_load_buffer_wav("@/sounds/bgm.wav");

// 设置音频缓冲区
bool changed = jeal_set_source_buffer(source, buffer);

// 播放
jeal_play_source(source);
```

## 注意事项

- 如果设置的音频与先前的音频不同，会自动停止当前播放
- 必须先设置缓冲区才能进行播放操作

## 相关接口

- [jeal_create_source](jeal_create_source.md) - 创建音频源
- [jeal_play_source](jeal_play_source.md) - 播放音频源
- [jeal_create_buffer](jeal_create_buffer.md) - 创建音频缓冲区
