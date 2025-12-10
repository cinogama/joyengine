# jeal_create_effect_eaxreverb

## 函数签名

```cpp
JE_API jeal_effect_eaxreverb* jeal_create_effect_eaxreverb();
```

## 描述

创建一个 EAX 混响音频效果实例。EAX 混响是一种增强型混响效果，提供比标准混响更多的参数控制。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| jeal_effect_eaxreverb* | EAX 混响效果实例指针 |

## 注意事项

- 创建后需要通过 `jeal_effect_slot_bind` 绑定到效果槽才能应用到音频源
- EAX 混响提供更精细的环境模拟控制

## 相关接口

- [jeal_create_effect_reverb](jeal_create_effect_reverb.md) - 创建标准混响效果
- [jeal_close_effect](jeal_close_effect.md) - 关闭效果
- [jeal_update_effect](jeal_update_effect.md) - 更新效果参数
- [jeal_effect_slot_bind](jeal_effect_slot_bind.md) - 绑定效果到效果槽
