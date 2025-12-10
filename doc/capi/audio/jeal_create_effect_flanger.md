# jeal_create_effect_flanger

## 函数签名

```cpp
JE_API jeal_effect_flanger* jeal_create_effect_flanger();
```

## 描述

创建一个镶边效果实例。镶边通过混合延迟时间变化的原始信号，产生类似"喷气机"或太空感的波动音效。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| jeal_effect_flanger* | 镶边效果实例指针 |

### jeal_effect_flanger 结构体

```cpp
struct jeal_effect_flanger {
    typedef jeal_effect_chorus::waveform waveform;
    waveform m_waveform; // 波形类型，默认 TRIANGLE
    int m_phase;         // 相位，默认 0，范围 [-180, 180]
    float m_rate;        // 速率，默认 0.27，范围 [0.0, 10.0]
    float m_depth;       // 深度，默认 1.0，范围 [0.0, 1.0]
    float m_feedback;    // 反馈，默认 -0.5，范围 [-1.0, 1.0]
    float m_delay;       // 延迟，默认 0.002，范围 [0.0, 0.004]
};
```

## 用法示例

```cpp
jeal_effect_flanger* flanger = jeal_create_effect_flanger();

// 调整镶边参数
flanger->m_rate = 0.5f;
flanger->m_depth = 0.8f;

jeal_update_effect(flanger);

// 绑定到效果槽
jeal_effect_slot_bind(slot, flanger);

// 清理
jeal_close_effect(flanger);
```

## 相关接口

- [jeal_update_effect](jeal_update_effect.md) - 更新效果参数
- [jeal_close_effect](jeal_close_effect.md) - 关闭效果
