# je_animation_entity_crossfade

## 函数签名

```c
JE_API void je_animation_entity_crossfade(
    const je_game_entity_t* entity,
    const char* action_name,
    float fade_duration);
```

## 描述

在 fade_duration 秒内从当前动作平滑过渡到指定动作。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `entity` | `const je_game_entity_t*` | 持有 Animation::Animator 的实体 |
| `action_name` | `const char*` | 目标动作名 |
| `fade_duration` | `float` | 过渡时长（秒） |

## 返回值

| 类型 | 描述 |
|------|------|
| ``void`` | — |

## 注意事项

- 当前无动作、已结束或时长为 0 时等价于 je_animation_entity_play
- 过渡按采样权重混合：同目标轨道插值过渡，叠加轨道按 value*weight 累加
- 过渡目标动作不存在时放弃过渡并保持当前动作
