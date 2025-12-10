# jeal_create_filter

## 函数签名

```cpp
jeal_filter* jeal_create_filter();
```

## 描述

创建一个滤波器。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| jeal_filter* | 滤波器实例指针 |

### jeal_filter 结构体

```cpp
struct jeal_filter {
    jeal_native_filter_instance* m_filter_instance;
    size_t m_references;     // 引用计数
    jeal_filter_type m_type; // 滤波器类型
    float m_gain;            // 增益，默认 1.0，范围 [0.0, 1.0]
    float m_gain_lf;         // 低频增益（对高通无效），默认 0.5
    float m_gain_hf;         // 高频增益（对低通无效），默认 0.5
};
```

### jeal_filter_type 枚举值

| 值 | 描述 |
|----|------|
| JE_AUDIO_FILTER_LOWPASS | 低通滤波器 |
| JE_AUDIO_FILTER_HIGHPASS | 高通滤波器 |
| JE_AUDIO_FILTER_BANDPASS | 带通滤波器 |

## 用法示例

```cpp
jeal_filter* filter = jeal_create_filter();

// 设置为低通滤波器
filter->m_type = JE_AUDIO_FILTER_LOWPASS;
filter->m_gain = 1.0f;
filter->m_gain_hf = 0.3f;  // 削减高频

// 应用设置
jeal_update_filter(filter);

// 应用到音频源
jeal_set_source_filter(source, filter);

// 清理
jeal_close_filter(filter);
```

## 相关接口

- [jeal_update_filter](jeal_update_filter.md) - 应用滤波器设置
- [jeal_close_filter](jeal_close_filter.md) - 关闭滤波器
- [jeal_set_source_filter](jeal_set_source_filter.md) - 设置音频源滤波器
