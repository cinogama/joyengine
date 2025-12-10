# je_ecs_universe_get_real_deltatime

## 函数签名

```cpp
JE_API double je_ecs_universe_get_real_deltatime(void* universe);
```

## 描述

获取 Universe 的真实帧间隔时间（未经时间缩放）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| universe | void* | Universe 实例指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| double | 真实帧间隔时间（秒） |

## 相关接口

- [je_ecs_universe_get_frame_deltatime](je_ecs_universe_get_frame_deltatime.md) - 获取缩放后的帧间隔时间
- [je_ecs_universe_get_smooth_deltatime](je_ecs_universe_get_smooth_deltatime.md) - 获取平滑帧间隔时间
