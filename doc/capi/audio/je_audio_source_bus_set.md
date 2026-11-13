# je_audio_source_bus_set

## 函数签名

```cpp
JE_API void je_audio_source_bus_set(je_audio_source_t* source, /* OPTIONAL */ je_audio_bus_t* bus_may_null);
```

## 描述

设置音频源所属的音量总线。

声源的最终增益为 `声源增益 × 总线增益`（再乘监听者增益）。总线增益变化时，使用该总线的声源会自动重新提交增益。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | je_audio_source_t* | 音频源指针 |
| bus_may_null | je_audio_bus_t* | 由 `je_audio_bus_get` 取得的总线；`nullptr` = 不经过总线 |

## 返回值

无

## 用法示例

```cpp
je_audio_source_t* source = je_audio_source_create();

je_audio_source_bus_set(source, je_audio_bus_get("SFX"));
je_audio_source_buffer_set(source, buffer);
je_audio_source_play(source);

// 之后可以直接调总线音量
je_audio_bus_gain_set(je_audio_bus_get("SFX"), 0.6f);

// 移除总线
je_audio_source_bus_set(source, nullptr);
```

## 注意事项

- 总线指针进程内稳定，声源只保存指针，不增加引用计数
- 直接修改 `source->m_bus` 后仍需调用 `je_audio_source_update`；本函数是设置并立即提交的便捷入口

## 相关接口

- [je_audio_bus_get](je_audio_bus_get.md) - 取得音量总线
- [je_audio_source_update](je_audio_source_update.md) - 提交音频源参数
