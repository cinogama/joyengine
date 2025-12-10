# jeal_get_source_play_process

## 函数签名

```cpp
JE_API size_t jeal_get_source_play_process(jeal_source* source);
```

## 描述

获取当前音频源的播放位置，单位是字节。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | jeal_source* | 音频源指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| size_t | 当前播放位置（字节偏移） |

## 用法示例

```cpp
size_t offset = jeal_get_source_play_process(source);
printf("当前播放位置: %zu 字节\n", offset);
```

## 相关接口

- [jeal_set_source_play_process](jeal_set_source_play_process.md) - 设置播放位置
