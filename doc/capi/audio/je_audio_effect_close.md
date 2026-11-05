# je_audio_effect_close

## 函数签名

```cpp
JE_API void je_audio_effect_close(void* effect);
```

## 描述

关闭一个音频效果实例。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| effect | void* | 要关闭的效果实例指针 |

## 返回值

无

## 用法示例

```cpp
jeal_effect_reverb* reverb = je_audio_effect_reverb_create();

// 使用效果...

je_audio_effect_close(reverb);
```

## 注意事项

- 此函数用于关闭任何由 `je_audio_effect_*_create` 函数创建的效果实例

## 相关接口

- [je_audio_effect_reverb_create](je_audio_effect_reverb_create.md) - 创建混响效果
- [je_audio_effect_chorus_create](je_audio_effect_chorus_create.md) - 创建合唱效果
- [je_audio_effect_echo_create](je_audio_effect_echo_create.md) - 创建回声效果
