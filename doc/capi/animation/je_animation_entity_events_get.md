# je_animation_entity_events_get

## 函数签名

```c
JE_API size_t je_animation_entity_events_get(
    const je_game_entity_t* entity,
    /* OPTIONAL */ const char** out_event_names,
    size_t out_event_names_capacity);
```

## 描述

获取本帧内动画播放越过事件点而触发的事件名。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `entity` | `const je_game_entity_t*` | 持有 Animation::Animator 的实体 |
| `out_event_names` | `const char**` | 接收事件名的数组；仅当容量为 0（只查询数量）时可为空 |
| `out_event_names_capacity` | `size_t` | 数组容量 |

## 返回值

| 类型 | 描述 |
|------|------|
| `size_t` | 本帧事件总数；大于容量说明缓冲不足 |

## 注意事项

- 读取不消费事件，同一帧内可重复读取
- 事件在实体所在世界的下一次 AnimatorSystem 更新前有效
- 事件缓冲上限 32，溢出丢弃并记录警告
