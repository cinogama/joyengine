# je_ecs_universe_get_max_deltatime

## 函数签名

```cpp
JE_API double je_ecs_universe_get_max_deltatime(void* universe);
```

## 描述

获取 Universe 的最大帧间隔时间限制。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| universe | void* | Universe 实例指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| double | 最大帧间隔时间（秒） |

## 相关接口

- [je_ecs_universe_set_max_deltatime](je_ecs_universe_set_max_deltatime.md) - 设置最大帧间隔时间
