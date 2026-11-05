# je_audio_effect_reverb_create

## 函数签名

```cpp
JE_API jeal_effect_reverb* je_audio_effect_reverb_create();
```

## 描述

创建一个混响效果实例。混响模拟声音在封闭空间（如房间、大厅）中的反射效果，增加空间感。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| jeal_effect_reverb* | 混响效果实例指针 |

### jeal_effect_reverb 结构体

```cpp
struct jeal_effect_reverb {
    float m_density;                // 密度, 默认 1.0, 范围 [0.0, 1.0]
    float m_diffusion;              // 扩散度, 默认 1.0, 范围 [0.0, 1.0]
    float m_gain;                   // 增益, 默认 0.32, 范围 [0.0, 1.0]
    float m_gain_hf;                // 高频增益, 默认 0.89, 范围 [0.0, 1.0]
    float m_decay_time;             // 衰减时间, 默认 1.49, 范围 [0.1, 20.0]
    float m_decay_hf_ratio;         // 高频衰减比率, 默认 0.83, 范围 [0.1, 2.0]
    float m_reflections_gain;       // 反射增益, 默认 0.05, 范围 [0.0, 3.16]
    float m_reflections_delay;      // 反射延迟, 默认 0.007, 范围 [0.0, 0.3]
    float m_late_reverb_gain;       // 后混响增益, 默认 1.26, 范围 [0.0, 10.0]
    float m_late_reverb_delay;      // 后混响延迟, 默认 0.011, 范围 [0.0, 0.1]
    float m_air_absorption_gain_hf; // 高频吸收, 默认 0.994, 范围 [0.892, 1.0]
    float m_room_rolloff_factor;    // 房间衰减因子, 默认 0.0, 范围 [0.0, 10.0]
    bool m_decay_hf_limit;          // 高频衰减限制器, 默认 false
};
```

## 用法示例

```cpp
jeal_effect_reverb* reverb = je_audio_effect_reverb_create();

// 调整混响参数（模拟大厅）
reverb->m_decay_time = 3.0f;
reverb->m_diffusion = 0.8f;

// 应用更改
je_audio_effect_update(reverb);

// 绑定到效果槽
jeal_effect_slot* slot = je_audio_effect_slot_create();
je_audio_effect_slot_bind(slot, reverb);

// 清理
je_audio_effect_close(reverb);
je_audio_effect_slot_close(slot);
```

## 相关接口

- [je_audio_effect_update](je_audio_effect_update.md) - 更新效果参数
- [je_audio_effect_close](je_audio_effect_close.md) - 关闭效果
- [je_audio_effect_slot_bind](je_audio_effect_slot_bind.md) - 绑定效果到效果槽
- [je_audio_effect_eaxreverb_create](je_audio_effect_eaxreverb_create.md) - 创建扩展 EAX 混响
