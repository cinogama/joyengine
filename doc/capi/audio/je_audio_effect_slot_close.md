# je_audio_effect_slot_close

## 函数签名

```cpp
JE_API void je_audio_effect_slot_close(je_audio_effect_slot_t* slot);
```

## 描述

关闭一个效果槽。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| slot | je_audio_effect_slot_t* | 要关闭的效果槽指针 |

## 返回值

无

## 用法示例

```cpp
je_audio_effect_slot_t* slot = je_audio_effect_slot_create();

// 使用效果槽...

je_audio_effect_slot_close(slot);
```

## 相关接口

- [je_audio_effect_slot_create](je_audio_effect_slot_create.md) - 创建效果槽
