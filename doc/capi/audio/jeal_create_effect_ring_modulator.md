# jeal_create_effect_ring_modulator

## 函数签名

```cpp
JE_API jeal_effect_ring_modulator* jeal_create_effect_ring_modulator();
```

## 描述

创建一个环形调制音频效果实例。环形调制效果可以产生类似机器人或金属质感的声音。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| jeal_effect_ring_modulator* | 环形调制效果实例指针 |

## 注意事项

- 创建后需要通过 `jeal_effect_slot_bind` 绑定到效果槽才能应用到音频源

## 相关接口

- [jeal_close_effect](jeal_close_effect.md) - 关闭效果
- [jeal_update_effect](jeal_update_effect.md) - 更新效果参数
- [jeal_effect_slot_bind](jeal_effect_slot_bind.md) - 绑定效果到效果槽
