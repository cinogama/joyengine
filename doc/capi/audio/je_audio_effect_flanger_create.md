# je_audio_effect_flanger_create

## 函数签名

```cpp
JE_API je_audio_effect_flanger_t* je_audio_effect_flanger_create();
```

## 描述

创建一个镶边效果实例。镶边通过混合延迟时间变化的原始信号，产生类似"喷气机"或太空感的波动音效。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| je_audio_effect_flanger_t* | 镶边效果实例指针 |

### je_audio_effect_flanger_t 结构体

```cpp
typedef je_audio_effect_chorus_waveform_t je_audio_effect_flanger_waveform_t;

typedef struct je_audio_effect_flanger_t {
    je_audio_effect_flanger_waveform_t m_waveform; // 波形类型，默认 TRIANGLE
    int m_phase;         // 相位，默认 0，范围 [-180, 180]
    float m_rate;        // 速率，默认 0.27，范围 [0.0, 10.0]
    float m_depth;       // 深度，默认 1.0，范围 [0.0, 1.0]
    float m_feedback;    // 反馈，默认 -0.5，范围 [-1.0, 1.0]
    float m_delay;       // 延迟，默认 0.002，范围 [0.0, 0.004]
} je_audio_effect_flanger_t;
```

## 用法示例

```cpp
je_audio_effect_flanger_t* flanger = je_audio_effect_flanger_create();

// 调整镶边参数
flanger->m_rate = 0.5f;
flanger->m_depth = 0.8f;

je_audio_effect_update(flanger);

// 绑定到效果槽
je_audio_effect_slot_bind(slot, flanger);

// 清理
je_audio_effect_close(flanger);
```

## 相关接口

- [je_audio_effect_update](je_audio_effect_update.md) - 更新效果参数
- [je_audio_effect_close](je_audio_effect_close.md) - 关闭效果
