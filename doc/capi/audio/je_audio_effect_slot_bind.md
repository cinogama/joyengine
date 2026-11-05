# je_audio_effect_slot_bind

## 函数签名

```cpp
JE_API void je_audio_effect_slot_bind(jeal_effect_slot* slot, void* effect_may_null);
```

## 描述

绑定一个效果到效果槽。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| slot | jeal_effect_slot* | 效果槽指针 |
| effect_may_null | void* | 效果实例指针，传入 nullptr 表示解除绑定 |

## 返回值

无

## 用法示例

```cpp
jeal_effect_slot* slot = je_audio_effect_slot_create();
jeal_effect_reverb* reverb = je_audio_effect_reverb_create();

// 绑定混响效果
je_audio_effect_slot_bind(slot, reverb);

// 解除绑定
je_audio_effect_slot_bind(slot, nullptr);
```

## 注意事项

- 效果可以是任意 `je_audio_effect_*_create` 函数创建的效果实例
- 传入 nullptr 可以解除当前绑定的效果

## 相关接口

- [je_audio_effect_slot_create](je_audio_effect_slot_create.md) - 创建效果槽
- [je_audio_effect_reverb_create](je_audio_effect_reverb_create.md) - 创建混响效果
