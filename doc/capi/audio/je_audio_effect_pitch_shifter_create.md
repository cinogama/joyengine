# je_audio_effect_pitch_shifter_create

## 函数签名

```cpp
JE_API je_audio_effect_pitch_shifter_t* je_audio_effect_pitch_shifter_create();
```

## 描述

创建一个音高偏移音频效果实例。音高偏移效果可以在不改变播放速度的情况下改变音频的音高。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| je_audio_effect_pitch_shifter_t* | 音高偏移效果实例指针 |

## 注意事项

- 创建后需要通过 `je_audio_effect_slot_bind` 绑定到效果槽才能应用到音频源

## 相关接口

- [je_audio_effect_close](je_audio_effect_close.md) - 关闭效果
- [je_audio_effect_update](je_audio_effect_update.md) - 更新效果参数
- [je_audio_effect_slot_bind](je_audio_effect_slot_bind.md) - 绑定效果到效果槽
