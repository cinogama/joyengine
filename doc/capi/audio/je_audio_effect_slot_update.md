# je_audio_effect_slot_update

## 函数签名

```cpp
JE_API void je_audio_effect_slot_update(je_audio_effect_slot_t* slot);
```

## 描述

将对 `je_audio_effect_slot_t` 的参数修改应用到实际的效果槽上。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| slot | je_audio_effect_slot_t* | 效果槽指针 |

## 返回值

无

## 用法示例

```cpp
je_audio_effect_slot_t* slot = je_audio_effect_slot_create();

// 修改增益
slot->m_gain = 0.5f;

// 应用更改
je_audio_effect_slot_update(slot);
```

## 相关接口

- [je_audio_effect_slot_create](je_audio_effect_slot_create.md) - 创建效果槽
