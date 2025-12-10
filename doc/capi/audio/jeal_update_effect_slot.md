# jeal_update_effect_slot

## 函数签名

```cpp
JE_API void jeal_update_effect_slot(jeal_effect_slot* slot);
```

## 描述

将对 `jeal_effect_slot` 的参数修改应用到实际的效果槽上。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| slot | jeal_effect_slot* | 效果槽指针 |

## 返回值

无

## 用法示例

```cpp
jeal_effect_slot* slot = jeal_create_effect_slot();

// 修改增益
slot->m_gain = 0.5f;

// 应用更改
jeal_update_effect_slot(slot);
```

## 相关接口

- [jeal_create_effect_slot](jeal_create_effect_slot.md) - 创建效果槽
