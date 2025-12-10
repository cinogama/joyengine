# je_ecs_universe_set_time_scale

## 函数签名

```cpp
JE_API void je_ecs_universe_set_time_scale(void* universe, double scale);
```

## 描述

设置 Universe 的时间缩放。用于实现慢动作或加速效果。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| universe | void* | Universe 实例指针 |
| scale | double | 时间缩放比例（1.0 为正常速度） |

## 返回值

无

## 用法示例

```cpp
// 慢动作（半速）
je_ecs_universe_set_time_scale(universe, 0.5);

// 加速（2倍速）
je_ecs_universe_set_time_scale(universe, 2.0);

// 暂停
je_ecs_universe_set_time_scale(universe, 0.0);
```

## 相关接口

- [je_ecs_universe_get_time_scale](je_ecs_universe_get_time_scale.md) - 获取时间缩放
