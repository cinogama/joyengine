# jeal_create_effect_distortion

## 函数签名

```cpp
JE_API jeal_effect_distortion* jeal_create_effect_distortion();
```

## 描述

创建一个失真效果实例。失真故意扭曲音频波形，常见于电吉他等乐器，制造粗糙或过载的音色。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| jeal_effect_distortion* | 失真效果实例指针 |

### jeal_effect_distortion 结构体

```cpp
struct jeal_effect_distortion {
    float m_edge;                  // 边缘，默认 0.2，范围 [0.0, 1.0]
    float m_gain;                  // 增益，默认 0.05，范围 [0.01, 1.0]
    float m_lowpass_cutoff;        // 低通截止频率，默认 8000.0，范围 [80.0, 24000.0]
    float m_equalizer_center_freq; // 均衡器中心频率，默认 3600.0，范围 [80.0, 24000.0]
    float m_equalizer_bandwidth;   // 均衡器带宽，默认 3600.0，范围 [80.0, 24000.0]
};
```

## 用法示例

```cpp
jeal_effect_distortion* distortion = jeal_create_effect_distortion();

// 调整失真参数
distortion->m_edge = 0.5f;
distortion->m_gain = 0.1f;

jeal_update_effect(distortion);

// 绑定到效果槽
jeal_effect_slot_bind(slot, distortion);

// 清理
jeal_close_effect(distortion);
```

## 相关接口

- [jeal_update_effect](jeal_update_effect.md) - 更新效果参数
- [jeal_close_effect](jeal_close_effect.md) - 关闭效果
