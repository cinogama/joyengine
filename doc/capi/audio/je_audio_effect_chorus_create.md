# je_audio_effect_chorus_create

## 函数签名

```cpp
JE_API je_audio_effect_chorus_t* je_audio_effect_chorus_create();
```

## 描述

创建一个合唱效果实例。合唱通过轻微延迟和音高变化复制原始声音，产生多个声部同时发声的效果。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| je_audio_effect_chorus_t* | 合唱效果实例指针 |

### je_audio_effect_chorus_t 结构体

```cpp
typedef enum je_audio_effect_chorus_waveform_t {
    JE_AUDIO_EFFECT_CHORUS_WAVEFORM_SINUSOID = 0,
    JE_AUDIO_EFFECT_CHORUS_WAVEFORM_TRIANGLE = 1,
} je_audio_effect_chorus_waveform_t;

typedef struct je_audio_effect_chorus_t {
    je_audio_effect_chorus_waveform_t m_waveform; // 波形类型，默认 SINUSOID
    int m_phase;         // 相位，默认 90，范围 [-180, 180]
    float m_rate;        // 速率，默认 1.1，范围 [0.0, 10.0]
    float m_depth;       // 深度，默认 0.1，范围 [0.0, 1.0]
    float m_feedback;    // 反馈，默认 0.25，范围 [-1.0, 1.0]
    float m_delay;       // 延迟，默认 0.16，范围 [0.0, 0.016]
} je_audio_effect_chorus_t;
```

## 用法示例

```cpp
je_audio_effect_chorus_t* chorus = je_audio_effect_chorus_create();

// 调整合唱参数
chorus->m_depth = 0.2f;
chorus->m_rate = 1.5f;

je_audio_effect_update(chorus);

// 绑定到效果槽
je_audio_effect_slot_bind(slot, chorus);

// 清理
je_audio_effect_close(chorus);
```

## 相关接口

- [je_audio_effect_update](je_audio_effect_update.md) - 更新效果参数
- [je_audio_effect_close](je_audio_effect_close.md) - 关闭效果
