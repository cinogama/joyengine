# je_animation_entity_pause

## 函数签名

```c
JE_API void je_animation_entity_pause(const je_game_entity_t* entity);
```

## 描述

暂停播放（冻结在当前姿态，时间与淡入淡出均不再推进）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `entity` | `const je_game_entity_t*` | 持有 Animation::Animator 的实体 |

## 返回值

| 类型 | 描述 |
|------|------|
| ``void`` | — |

## 注意事项

