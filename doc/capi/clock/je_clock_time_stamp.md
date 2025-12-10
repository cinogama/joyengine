# je_clock_time_stamp

## 函数签名

```c
JE_API jeecs::typing::timestamp_ms_t je_clock_time_stamp();
```

## 描述

获取当前时间戳，单位是毫秒。

## 参数

无参数。

## 返回值

| 类型 | 描述 |
|------|------|
| `jeecs::typing::timestamp_ms_t` | 当前时间戳（毫秒） |

## 用法

此函数用于获取系统时间戳，通常用于需要绝对时间的场景。

### 示例

```cpp
// 获取当前时间戳
jeecs::typing::timestamp_ms_t now = je_clock_time_stamp();
printf("Current timestamp: %llu ms\n", (unsigned long long)now);
```

## 注意事项

- 这个时间与 `je_clock_time` 获取到的时间不同
- **不是**引擎启动时起的计时，而是系统时间戳
- 类型 `timestamp_ms_t` 是 `uint64_t` 的别名

## 相关接口

- [je_clock_time](je_clock_time.md) - 获取引擎运行时间
