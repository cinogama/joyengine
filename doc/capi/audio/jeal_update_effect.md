# jeal_update_effect

## 函数签名

```cpp
JE_API void jeal_update_effect(void* effect);
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
jeal_effect_reverb* reverb = jeal_create_effect_reverb();

// 修改参数
reverb->m_decay_time = 2.5f;

// 应用更改
jeal_update_effect(reverb);
```

## 注意事项

- 对效果的更新不会应用到已经绑定的效果槽和音频源上，需要通过 `jeal_effect_slot_bind` 重新绑定

## 相关接口

- [jeal_effect_slot_bind](jeal_effect_slot_bind.md) - 绑定效果到效果槽
