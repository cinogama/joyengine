# je_ecs_universe_frame_deltatime_get

## 函数签名

```cpp
JE_API double je_ecs_universe_frame_deltatime_get(void* universe);
```

## 描述

获取 Universe 的帧间隔时间（经过时间缩放）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| universe | void* | Universe 实例指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| double | 帧间隔时间（秒），已应用时间缩放 |

## 相关接口

- [je_ecs_universe_frame_deltatime_set](je_ecs_universe_frame_deltatime_set.md) - 设置帧间隔时间
- [je_ecs_universe_real_deltatime_get](je_ecs_universe_real_deltatime_get.md) - 获取真实帧间隔时间
- [je_ecs_universe_time_scale_get](je_ecs_universe_time_scale_get.md) - 获取时间缩放
