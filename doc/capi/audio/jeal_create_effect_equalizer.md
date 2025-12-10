# jeal_create_effect_equalizer

## 函数签名

```cpp
JE_API jeal_effect_equalizer* jeal_create_effect_equalizer();
```

## 描述

创建一个均衡器音频效果实例。均衡器效果可以调节不同频段的增益。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| jeal_effect_equalizer* | 均衡器效果实例指针 |

## 注意事项

- 创建后需要通过 `jeal_effect_slot_bind` 绑定到效果槽才能应用到音频源

## 相关接口

- [jeal_close_effect](jeal_close_effect.md) - 关闭效果
- [jeal_update_effect](jeal_update_effect.md) - 更新效果参数
- [jeal_effect_slot_bind](jeal_effect_slot_bind.md) - 绑定效果到效果槽
