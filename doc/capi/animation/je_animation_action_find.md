# je_animation_action_find

## 函数签名

```c
JE_API /* OPTIONAL */ const je_animation_action_data_t* je_animation_action_find(
    const je_animation_t* animation,
    const char* name);
```

## 描述

按名称查找动作，找不到返回 nullptr

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `animation` | `const je_animation_t*` | — |
| `name` | `const char*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `const je_animation_action_data_t*` | 见描述。 |
