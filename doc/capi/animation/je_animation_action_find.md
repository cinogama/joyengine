# je_animation_action_find

## 函数签名

```c
JE_API /* OPTIONAL */ const je_animation_action_data_t* je_animation_action_find(
    const je_animation_t* animation,
    const char* name);
```

## 描述

按名称查找动作，找不到返回 nullptr。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `animation` | `const je_animation_t*` | 动画集 |
| `name` | `const char*` | 动作名 |

## 返回值

| 类型 | 描述 |
|------|------|
| `const je_animation_action_data_t*` | 动作数据（含时长、循环方式、轨道与事件）；未找到为 nullptr |

## 注意事项

- 线性查找，动作数量大时可自行建立索引
