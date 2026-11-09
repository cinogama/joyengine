# je_audio_effect_compressor_create

## 函数签名

```cpp
JE_API je_audio_effect_compressor_t* je_audio_effect_compressor_create();
```

## 描述

创建一个压缩器音频效果实例。压缩器效果可以减小音频信号的动态范围。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| je_audio_effect_compressor_t* | 压缩器效果实例指针 |

## 注意事项

- 创建后需要通过 `je_audio_effect_slot_bind` 绑定到效果槽才能应用到音频源

## 相关接口

- [je_audio_effect_close](je_audio_effect_close.md) - 关闭效果
- [je_audio_effect_update](je_audio_effect_update.md) - 更新效果参数
- [je_audio_effect_slot_bind](je_audio_effect_slot_bind.md) - 绑定效果到效果槽
