# je_ecs_universe_smooth_deltatime_get

## 函数签名

```cpp
JE_API double je_ecs_universe_smooth_deltatime_get(void* universe);
```

## 描述

获取 Universe 的平滑帧间隔时间。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| universe | void* | Universe 实例指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| double | 平滑帧间隔时间（秒） |

## 相关接口

- [je_ecs_universe_real_deltatime_get](je_ecs_universe_real_deltatime_get.md) - 获取真实帧间隔时间
