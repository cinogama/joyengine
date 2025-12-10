# jeal_set_source_filter

## 函数签名

```cpp
JE_API void jeal_set_source_filter(jeal_source* source, jeal_filter* filter_may_null);
```

## 描述

设置音频源的滤波器。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | jeal_source* | 音频源指针 |
| filter_may_null | jeal_filter* | 滤波器指针，可以为 nullptr 表示移除滤波器 |

## 返回值

无

## 用法示例

```cpp
jeal_source* source = jeal_create_source();
jeal_filter* filter = jeal_create_filter();

// 设置为低通滤波器
filter->m_type = JE_AUDIO_FILTER_LOWPASS;
filter->m_gain_hf = 0.5f;  // 削减高频
jeal_update_filter(filter);

// 应用滤波器
jeal_set_source_filter(source, filter);

// 移除滤波器
jeal_set_source_filter(source, nullptr);
```

## 相关接口

- [jeal_create_filter](jeal_create_filter.md) - 创建滤波器
- [jeal_set_source_effect_slot_and_filter](jeal_set_source_effect_slot_and_filter.md) - 设置效果槽和滤波器
