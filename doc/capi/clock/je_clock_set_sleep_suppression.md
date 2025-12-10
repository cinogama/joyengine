# je_clock_set_sleep_suppression

## 函数签名

```c
JE_API void je_clock_set_sleep_suppression(double v);
```

## 描述

设置引擎的提前唤醒间隔，单位是秒。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `v` | `double` | 提前唤醒间隔（秒） |

## 返回值

无返回值。

## 用法

此值影响 sleep 操作，会减少 sleep 的实际时间，有助于提升画面的流畅度。

### 示例

```cpp
// 设置 2ms 的提前唤醒时间
je_clock_set_sleep_suppression(0.002);
```

## 注意事项

- 增加此值会提升帧率稳定性，但会增加 CPU 空转导致的开销
- 虽然不影响实际性能，但会增加功耗
- 默认值通常已经能满足大多数需求

## 相关接口

- [je_clock_get_sleep_suppression](je_clock_get_sleep_suppression.md) - 获取提前唤醒间隔
