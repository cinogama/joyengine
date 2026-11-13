# je_audio_source_update

## 函数签名

```cpp
JE_API void je_audio_source_update(je_audio_source_t* source);
```

## 描述

将对 `je_audio_source_t` 的参数修改应用到实际的音频源上。

可提交的参数包括：循环、音量、音调、位置、速度、距离衰减参数（`m_reference_distance` / `m_max_distance` / `m_rolloff` / `m_min_gain` / `m_max_gain`）、`m_relative`、锥形参数与音量总线。距离模型与多普勒系数属于监听者（上下文级）状态，不在本函数中提交。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | je_audio_source_t* | 音频源指针 |

## 返回值

无

## 用法示例

```cpp
je_audio_source_t* source = je_audio_source_create();

// 修改属性
source->m_gain = 0.5f;              // 设置音量为 50%
source->m_pitch = 1.2f;             // 加速播放
source->m_reference_distance = 1.f; // 参考距离
source->m_max_distance = 32.f;      // 最大距离
source->m_rolloff = 1.f;            // 衰减率

// 应用修改
je_audio_source_update(source);
```

## 注意事项

- 直接修改 `je_audio_source_t` 结构体的成员不会立即生效，需要调用此函数
- 可在播放过程中动态调整音频属性
- 距离衰减与锥形只对 `m_relative == false` 的声源生效

## 相关接口

- [je_audio_source_create](je_audio_source_create.md) - 创建音频源
- [je_audio_source_close](je_audio_source_close.md) - 释放音频源
- [je_audio_source_bus_set](je_audio_source_bus_set.md) - 设置音量总线
- [je_audio_listener_update](je_audio_listener_update.md) - 提交距离模型与多普勒系数
