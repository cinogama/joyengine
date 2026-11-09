# je_audio_source_filter_set

## 函数签名

```cpp
JE_API void je_audio_source_filter_set(je_audio_source_t* source, je_audio_filter_t* filter_may_null);
```

## 描述

设置音频源的滤波器。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | je_audio_source_t* | 音频源指针 |
| filter_may_null | je_audio_filter_t* | 滤波器指针，可以为 nullptr 表示移除滤波器 |

## 返回值

无

## 用法示例

```cpp
je_audio_source_t* source = je_audio_source_create();
je_audio_filter_t* filter = je_audio_filter_create();

// 设置为低通滤波器
filter->m_type = JE_AUDIO_FILTER_LOWPASS;
filter->m_gain_hf = 0.5f;  // 削减高频
je_audio_filter_update(filter);

// 应用滤波器
je_audio_source_filter_set(source, filter);

// 移除滤波器
je_audio_source_filter_set(source, nullptr);
```

## 相关接口

- [je_audio_filter_create](je_audio_filter_create.md) - 创建滤波器
- [je_audio_source_effect_slot_and_filter_set](je_audio_source_effect_slot_and_filter_set.md) - 设置效果槽和滤波器
