# jeal_close_effect

## 函数签名

```cpp
JE_API void jeal_close_effect(void* effect);
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
jeal_effect_reverb* reverb = jeal_create_effect_reverb();

// 使用效果...

jeal_close_effect(reverb);
```

## 注意事项

- 此函数用于关闭任何由 `jeal_create_effect_*` 函数创建的效果实例

## 相关接口

- [jeal_create_effect_reverb](jeal_create_effect_reverb.md) - 创建混响效果
- [jeal_create_effect_chorus](jeal_create_effect_chorus.md) - 创建合唱效果
- [jeal_create_effect_echo](jeal_create_effect_echo.md) - 创建回声效果
