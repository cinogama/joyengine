# je_animation_entity_stop

## 函数签名

```c
JE_API void je_animation_entity_stop(const je_game_entity_t* entity);
```

## 描述

停止播放：清除动作与进度，清除淡入淡出。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `entity` | `const je_game_entity_t*` | 持有 Animation::Animator 的实体 |

## 返回值

| 类型 | 描述 |
|------|------|
| ``void`` | — |

## 注意事项

- 停止后不再向目标写值；最后写入的值保持
