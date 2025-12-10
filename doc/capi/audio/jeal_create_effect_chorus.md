# jeal_create_effect_chorus

## 函数签名

```cpp
JE_API jeal_effect_chorus* jeal_create_effect_chorus();
```

## 描述

创建一个合唱效果实例。合唱通过轻微延迟和音高变化复制原始声音，产生多个声部同时发声的效果。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| jeal_effect_chorus* | 合唱效果实例指针 |

### jeal_effect_chorus 结构体

```cpp
struct jeal_effect_chorus {
    enum waveform { SINUSOID = 0, TRIANGLE = 1 };
    waveform m_waveform; // 波形类型，默认 SINUSOID
    int m_phase;         // 相位，默认 90，范围 [-180, 180]
    float m_rate;        // 速率，默认 1.1，范围 [0.0, 10.0]
    float m_depth;       // 深度，默认 0.1，范围 [0.0, 1.0]
    float m_feedback;    // 反馈，默认 0.25，范围 [-1.0, 1.0]
    float m_delay;       // 延迟，默认 0.16，范围 [0.0, 0.016]
};
```

## 用法示例

```cpp
jeal_effect_chorus* chorus = jeal_create_effect_chorus();

// 调整合唱参数
chorus->m_depth = 0.2f;
chorus->m_rate = 1.5f;

jeal_update_effect(chorus);

// 绑定到效果槽
jeal_effect_slot_bind(slot, chorus);

// 清理
jeal_close_effect(chorus);
```

## 相关接口

- [jeal_update_effect](jeal_update_effect.md) - 更新效果参数
- [jeal_close_effect](jeal_close_effect.md) - 关闭效果
