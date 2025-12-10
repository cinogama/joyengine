# jeal_stop_source

## 函数签名

```cpp
JE_API void jeal_stop_source(jeal_source* source);
```

## 描述

停止音频源的播放。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | jeal_source* | 音频源指针 |

## 返回值

无

## 用法示例

```cpp
// 停止播放
jeal_stop_source(source);

// 重新播放将从头开始
jeal_play_source(source);
```

## 注意事项

- 停止后调用 `jeal_play_source` 会从音频起始位置重新播放

## 相关接口

- [jeal_play_source](jeal_play_source.md) - 播放音频源
- [jeal_pause_source](jeal_pause_source.md) - 暂停播放
