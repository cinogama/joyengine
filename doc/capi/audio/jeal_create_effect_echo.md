# jeal_create_effect_echo

## 函数签名

```cpp
JE_API jeal_effect_echo* jeal_create_effect_echo();
```

## 描述

创建一个回声效果实例。回声模拟声音在远处反射后延迟返回的效果（如山谷中的回声）。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| jeal_effect_echo* | 回声效果实例指针 |

### jeal_effect_echo 结构体

```cpp
struct jeal_effect_echo {
    float m_delay;    // 延迟时间，默认 0.1，范围 [0.0, 0.207]
    float m_lr_delay; // 左声道延迟，默认 0.1，范围 [0.0, 0.404]
    float m_damping;  // 阻尼，默认 0.5，范围 [0.0, 0.99]
    float m_feedback; // 反馈，默认 0.5，范围 [0.0, 1.0]
    float m_spread;   // 扩散，默认 -1.0，范围 [-1.0, 1.0]
};
```

## 用法示例

```cpp
jeal_effect_echo* echo = jeal_create_effect_echo();

// 调整回声参数
echo->m_delay = 0.15f;
echo->m_feedback = 0.3f;

jeal_update_effect(echo);

// 绑定到效果槽
jeal_effect_slot_bind(slot, echo);

// 清理
jeal_close_effect(echo);
```

## 相关接口

- [jeal_update_effect](jeal_update_effect.md) - 更新效果参数
- [jeal_close_effect](jeal_close_effect.md) - 关闭效果
