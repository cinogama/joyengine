# je_animation_entity_play

## 函数签名

```c
JE_API void je_animation_entity_play(const je_game_entity_t* entity, const char* action_name);
```

## 描述

立即播放实体 Animation::Animator 组件上的指定动作，重置进度并清除淡入淡出。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `entity` | `const je_game_entity_t*` | 持有 Animation::Animator 的实体 |
| `action_name` | `const char*` | 动作名 |

## 返回值

| 类型 | 描述 |
|------|------|
| ``void`` | — |

## 注意事项

- 由 Animation::AnimatorSystem 在下一帧开始推进
- 未找到动作时系统记录警告并停止播放
