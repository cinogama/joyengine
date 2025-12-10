# je_clock_sleep_for

## 函数签名

```c
JE_API void je_clock_sleep_for(double time);
```

## 描述

挂起当前线程若干秒。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `time` | `double` | 需要休眠的时间（秒） |

## 返回值

无返回值。

## 用法

此函数用于在当前线程上执行延时操作。

### 示例

```cpp
printf("Starting...\n");

// 休眠 0.5 秒
je_clock_sleep_for(0.5);

printf("500ms later...\n");
```

## 注意事项

- 实际休眠时间可能略有误差，取决于系统调度
- 休眠时间会受到 `je_clock_set_sleep_suppression` 设置的影响
- 不要在主逻辑线程中使用长时间休眠

## 相关接口

- [je_clock_sleep_until](je_clock_sleep_until.md) - 休眠直到指定时间
- [je_clock_get_sleep_suppression](je_clock_get_sleep_suppression.md) - 获取提前唤醒间隔
- [je_clock_set_sleep_suppression](je_clock_set_sleep_suppression.md) - 设置提前唤醒间隔
