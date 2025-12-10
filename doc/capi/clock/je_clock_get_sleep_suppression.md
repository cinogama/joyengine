# je_clock_get_sleep_suppression

## 函数签名

```c
JE_API double je_clock_get_sleep_suppression();
```

## 描述

获取引擎的提前唤醒间隔，单位是秒。

## 参数

无参数。

## 返回值

| 类型 | 描述 |
|------|------|
| `double` | 提前唤醒间隔（秒） |

## 用法

此值影响 sleep 操作，会减少 sleep 的实际时间。

### 示例

```cpp
double suppression = je_clock_get_sleep_suppression();
printf("Sleep suppression: %.3f seconds\n", suppression);
```

## 注意事项

- 此值用于提升画面流畅度
- 较大的值会增加 CPU 空转，但帧率更稳定
- 较小的值降低 CPU 占用，但可能导致帧率波动

## 相关接口

- [je_clock_set_sleep_suppression](je_clock_set_sleep_suppression.md) - 设置提前唤醒间隔
- [je_clock_sleep_for](je_clock_sleep_for.md) - 休眠指定时长
