# je_ecs_universe_max_deltatime_set

## 函数签名

```cpp
JE_API void je_ecs_universe_max_deltatime_set(void* universe, double val);
```

## 描述

设置 Universe 的最大帧间隔时间限制。用于防止帧间隔时间过大导致的物理模拟等问题。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| universe | void* | Universe 实例指针 |
| val | double | 最大帧间隔时间（秒） |

## 返回值

无

## 相关接口

- [je_ecs_universe_max_deltatime_get](je_ecs_universe_max_deltatime_get.md) - 获取最大帧间隔时间
