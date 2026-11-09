# je_animation_entity_action_get

## 函数签名

```c
JE_API /* OPTIONAL */ const char* je_animation_entity_action_get(const je_game_entity_t* entity);
```

## 描述

获取当前动作名。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `entity` | `const je_game_entity_t*` | 持有 Animation::Animator 的实体 |

## 返回值

| 类型 | 描述 |
|------|------|
| `const char*` | 当前动作名；未播放时返回 nullptr |

## 注意事项

- 返回的指针在动作切换前有效（指向组件内部字符串）
