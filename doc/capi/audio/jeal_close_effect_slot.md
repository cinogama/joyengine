# jeal_close_effect_slot

## 函数签名

```cpp
JE_API void jeal_close_effect_slot(jeal_effect_slot* slot);
```

## 描述

关闭一个效果槽。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| slot | jeal_effect_slot* | 要关闭的效果槽指针 |

## 返回值

无

## 用法示例

```cpp
jeal_effect_slot* slot = jeal_create_effect_slot();

// 使用效果槽...

jeal_close_effect_slot(slot);
```

## 相关接口

- [jeal_create_effect_slot](jeal_create_effect_slot.md) - 创建效果槽
