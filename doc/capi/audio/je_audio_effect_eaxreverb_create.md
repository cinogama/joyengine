# je_audio_effect_eaxreverb_create

## 函数签名

```cpp
JE_API je_audio_effect_eax_reverb_t* je_audio_effect_eaxreverb_create();
```

## 描述

创建一个 EAX 混响音频效果实例。EAX 混响是一种增强型混响效果，提供比标准混响更多的参数控制。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| je_audio_effect_eax_reverb_t* | EAX 混响效果实例指针 |

## 注意事项

- 创建后需要通过 `je_audio_effect_slot_bind` 绑定到效果槽才能应用到音频源
- EAX 混响提供更精细的环境模拟控制

## 相关接口

- [je_audio_effect_reverb_create](je_audio_effect_reverb_create.md) - 创建标准混响效果
- [je_audio_effect_close](je_audio_effect_close.md) - 关闭效果
- [je_audio_effect_update](je_audio_effect_update.md) - 更新效果参数
- [je_audio_effect_slot_bind](je_audio_effect_slot_bind.md) - 绑定效果到效果槽
