# je_audio_source_effect_slot_and_filter_set

## 函数签名

```cpp
JE_API void je_audio_source_effect_slot_and_filter_set(
    je_audio_source_t* source,
    size_t slot_idx,
    je_audio_effect_slot_t* slot_may_null,
    je_audio_filter_t* filter_may_null);
```

## 描述

设置音频源的效果槽和滤波器。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | je_audio_source_t* | 音频源指针 |
| slot_idx | size_t | 效果槽索引，必须小于 `jeecs::audio::MAX_AUXILIARY_SENDS` |
| slot_may_null | je_audio_effect_slot_t* | 效果槽指针，可以为 nullptr |
| filter_may_null | je_audio_filter_t* | 滤波器指针，可以为 nullptr |

## 返回值

无

## 用法示例

```cpp
je_audio_source_t* source = je_audio_source_create();
je_audio_effect_slot_t* slot = je_audio_effect_slot_create();
je_audio_filter_t* filter = je_audio_filter_create();

// 创建混响效果
je_audio_effect_reverb_t* reverb = je_audio_effect_reverb_create();
je_audio_effect_slot_bind(slot, reverb);

// 设置滤波器为低通
filter->m_type = JE_AUDIO_FILTER_LOWPASS;
filter->m_gain_hf = 0.3f;
je_audio_filter_update(filter);

// 将效果槽和滤波器应用到音频源
je_audio_source_effect_slot_and_filter_set(source, 0, slot, filter);

// 清理
je_audio_effect_close(reverb);
je_audio_effect_slot_close(slot);
je_audio_filter_close(filter);
je_audio_source_close(source);
```

## 注意事项

- slot_idx 必须小于平台支持的最大辅助发送数量
- 传入 nullptr 可以解除对应的绑定

## 相关接口

- [je_audio_effect_slot_create](je_audio_effect_slot_create.md) - 创建效果槽
- [je_audio_filter_create](je_audio_filter_create.md) - 创建滤波器
- [je_audio_source_filter_set](je_audio_source_filter_set.md) - 仅设置滤波器
