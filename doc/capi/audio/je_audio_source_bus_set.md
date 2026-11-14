# je_audio_source_bus_set

## 函数签名

```cpp
JE_API void je_audio_source_bus_set(je_audio_source_t* source, je_audio_bus_t bus);
```

## 描述

设置音频源所属的音量总线。

声源的最终增益为 `声源增益 × 总线增益`（再乘监听者增益）。总线增益变化时，使用该总线的声源会自动重新提交增益。`je_audio_source_create` 新建的声源默认属于 `JE_AUDIO_BUS_NONE`（不经过总线）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | je_audio_source_t* | 音频源指针 |
| bus | je_audio_bus_t | 音量总线，取值见 [je_audio_bus_gain_set](je_audio_bus_gain_set.md) |

## 返回值

无

## 用法示例

```cpp
je_audio_source_t* source = je_audio_source_create();

je_audio_source_bus_set(source, JE_AUDIO_BUS_SE);
je_audio_source_buffer_set(source, buffer);
je_audio_source_play(source);

// 之后可以直接调总线音量
je_audio_bus_gain_set(JE_AUDIO_BUS_SE, 0.6f);

// 改为不经过总线
je_audio_source_bus_set(source, JE_AUDIO_BUS_NONE);
```

## 注意事项

- 直接修改 `source->m_bus` 后仍需调用 `je_audio_source_update`；本函数是设置并立即提交的便捷入口
- 组件 `Audio::Source` 的 `bus` 字段就是同一枚举，由音频系统在变化时提交

## 相关接口

- [je_audio_bus_gain_set](je_audio_bus_gain_set.md) - 设置总线增益（含总线枚举说明）
- [je_audio_source_update](je_audio_source_update.md) - 提交音频源参数
