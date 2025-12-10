# jeal_pause_source

## 函数签名

```cpp
JE_API void jeal_pause_source(jeal_source* source);
```

## 描述

暂停音频源的播放。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | jeal_source* | 音频源指针 |

## 返回值

无

## 用法示例

```cpp
// 暂停播放
jeal_pause_source(source);

// 继续播放
jeal_play_source(source);
```

## 注意事项

- 暂停后调用 `jeal_play_source` 会从暂停位置继续播放

## 相关接口

- [jeal_play_source](jeal_play_source.md) - 播放/继续播放
- [jeal_stop_source](jeal_stop_source.md) - 停止播放
