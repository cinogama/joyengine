# je_audio_effect_update

## 函数签名

```cpp
JE_API void je_audio_effect_update(void* effect);
```

## 描述

将对效果的参数修改应用到实际的效果上。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| effect | void* | 效果实例指针 |

## 返回值

无

## 用法示例

```cpp
je_audio_effect_reverb_t* reverb = je_audio_effect_reverb_create();

// 修改参数
reverb->m_decay_time = 2.5f;

// 应用更改
je_audio_effect_update(reverb);
```

## 注意事项

- 对效果的更新不会应用到已经绑定的效果槽和音频源上，需要通过 `je_audio_effect_slot_bind` 重新绑定

## 相关接口

- [je_audio_effect_slot_bind](je_audio_effect_slot_bind.md) - 绑定效果到效果槽
