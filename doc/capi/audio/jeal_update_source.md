# jeal_update_source

## 函数签名

```cpp
JE_API void jeal_update_source(jeal_source* source);
```

## 描述

将对 `jeal_source` 的参数修改应用到实际的音频源上。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | jeal_source* | 音频源指针 |

## 返回值

无

## 用法示例

```cpp
jeal_source* source = jeal_create_source();

// 修改属性
source->m_gain = 0.5f;  // 设置音量为 50%
source->m_pitch = 1.2f; // 加速播放

// 应用修改
jeal_update_source(source);
```

## 注意事项

- 直接修改 `jeal_source` 结构体的成员不会立即生效，需要调用此函数
- 可在播放过程中动态调整音频属性

## 相关接口

- [jeal_create_source](jeal_create_source.md) - 创建音频源
- [jeal_close_source](jeal_close_source.md) - 释放音频源
