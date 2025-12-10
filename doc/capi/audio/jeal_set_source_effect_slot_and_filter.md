# jeal_set_source_effect_slot_and_filter

## 函数签名

```cpp
JE_API void jeal_set_source_effect_slot_and_filter(
    jeal_source* source,
    size_t slot_idx,
    jeal_effect_slot* slot_may_null,
    jeal_filter* filter_may_null);
```

## 描述

设置音频源的效果槽和滤波器。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | jeal_source* | 音频源指针 |
| slot_idx | size_t | 效果槽索引，必须小于 `jeecs::audio::MAX_AUXILIARY_SENDS` |
| slot_may_null | jeal_effect_slot* | 效果槽指针，可以为 nullptr |
| filter_may_null | jeal_filter* | 滤波器指针，可以为 nullptr |

## 返回值

无

## 用法示例

```cpp
jeal_source* source = jeal_create_source();
jeal_effect_slot* slot = jeal_create_effect_slot();
jeal_filter* filter = jeal_create_filter();

// 创建混响效果
jeal_effect_reverb* reverb = jeal_create_effect_reverb();
jeal_effect_slot_bind(slot, reverb);

// 设置滤波器为低通
filter->m_type = JE_AUDIO_FILTER_LOWPASS;
filter->m_gain_hf = 0.3f;
jeal_update_filter(filter);

// 将效果槽和滤波器应用到音频源
jeal_set_source_effect_slot_and_filter(source, 0, slot, filter);

// 清理
jeal_close_effect(reverb);
jeal_close_effect_slot(slot);
jeal_close_filter(filter);
jeal_close_source(source);
```

## 注意事项

- slot_idx 必须小于平台支持的最大辅助发送数量
- 传入 nullptr 可以解除对应的绑定

## 相关接口

- [jeal_create_effect_slot](jeal_create_effect_slot.md) - 创建效果槽
- [jeal_create_filter](jeal_create_filter.md) - 创建滤波器
- [jeal_set_source_filter](jeal_set_source_filter.md) - 仅设置滤波器
