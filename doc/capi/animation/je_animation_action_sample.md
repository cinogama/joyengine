# je_animation_action_sample

## 函数签名

```c
JE_API void je_animation_action_sample(
    const je_animation_action_data_t* action,
    float time,
    float weight,
    je_animation_sample_buffer_t* buffer);
```

## 描述

按权重把 action 在 time 时刻的采样结果累加进 buffer。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `action` | `const je_animation_action_data_t*` | 被采样的动作 |
| `time` | `float` | 动作内时间（秒）；超出首/尾关键帧范围时钳制到端点值 |
| `weight` | `float` | 本次采样的权重；<= 0 时忽略 |
| `buffer` | `je_animation_sample_buffer_t*` | 采样累加缓冲 |

## 返回值

| 类型 | 描述 |
|------|------|
| ``void`` | — |

## 注意事项

- 多次采样的同一目标按权重加权平均（QUAT4 为 slerp），叠加轨道按 value*weight 累加
- time 按动作原始时间轴理解，不做循环回绕
- 轨道关键帧为空时该轨道不参与采样
