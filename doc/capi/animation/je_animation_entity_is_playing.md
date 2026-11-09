# je_animation_entity_is_playing

## 函数签名

```c
JE_API bool je_animation_entity_is_playing(const je_game_entity_t* entity);
```

## 描述

实体是否正在播放（已指定动作、未暂停且未结束）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `entity` | `const je_game_entity_t*` | 持有 Animation::Animator 的实体 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 正在播放为 true |

## 注意事项

- 非循环动作播放到结尾即为结束（is_playing 为 false，但仍保持最终姿态）
