# je_audio_filter_create

## 函数签名

```cpp
je_audio_filter_t* je_audio_filter_create();
```

## 描述

创建一个滤波器。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| je_audio_filter_t* | 滤波器实例指针 |

### je_audio_filter_t 结构体

```cpp
struct je_audio_filter_t {
    je_audio_native_filter_instance_t* m_filter_instance;
    size_t m_references;     // 引用计数
    je_audio_filter_type_t m_type; // 滤波器类型
    float m_gain;            // 增益，默认 1.0，范围 [0.0, 1.0]
    float m_gain_lf;         // 低频增益（对高通无效），默认 0.5
    float m_gain_hf;         // 高频增益（对低通无效），默认 0.5
};
```

### je_audio_filter_type_t 枚举值

| 值 | 描述 |
|----|------|
| JE_AUDIO_FILTER_LOWPASS | 低通滤波器 |
| JE_AUDIO_FILTER_HIGHPASS | 高通滤波器 |
| JE_AUDIO_FILTER_BANDPASS | 带通滤波器 |

## 用法示例

```cpp
je_audio_filter_t* filter = je_audio_filter_create();

// 设置为低通滤波器
filter->m_type = JE_AUDIO_FILTER_LOWPASS;
filter->m_gain = 1.0f;
filter->m_gain_hf = 0.3f;  // 削减高频

// 应用设置
je_audio_filter_update(filter);

// 应用到音频源
je_audio_source_filter_set(source, filter);

// 清理
je_audio_filter_close(filter);
```

## 相关接口

- [je_audio_filter_update](je_audio_filter_update.md) - 应用滤波器设置
- [je_audio_filter_close](je_audio_filter_close.md) - 关闭滤波器
- [je_audio_source_filter_set](je_audio_source_filter_set.md) - 设置音频源滤波器
