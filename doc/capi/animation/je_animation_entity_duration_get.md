# je_animation_entity_duration_get

## 函数签名

```c
JE_API float je_animation_entity_duration_get(const je_game_entity_t* entity);
```

## 描述

当前动作的总时长（秒）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `entity` | `const je_game_entity_t*` | 持有 Animation::Animator 的实体 |

## 返回值

| 类型 | 描述 |
|------|------|
| `float` | 总时长；未播放或动作缺失时为 0 |

## 注意事项
- 资源由 AnimatorSystem 惰性加载，首次更新前查询返回 0
