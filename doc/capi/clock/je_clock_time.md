# je_clock_time

## 函数签名

```c
JE_API double je_clock_time();
```

## 描述

获取引擎的运行时间戳，单位是秒。获取到的时间是引擎自启动（`je_init` 调用）到当前为止的时间。

## 参数

无参数。

## 返回值

| 类型 | 描述 |
|------|------|
| `double` | 引擎启动以来经过的秒数 |

## 用法

此函数用于获取高精度的引擎运行时间，常用于动画、计时器等场景。

### 示例

```cpp
// 记录开始时间
double start_time = je_clock_time();

// 执行某些操作...

// 计算经过时间
double elapsed = je_clock_time() - start_time;
printf("Operation took %.3f seconds\n", elapsed);
```

## 注意事项

- 返回的时间从 `je_init` 调用时开始计算
- 这是一个高精度时间，适合用于测量性能
- 此时间与系统时钟无关，不受系统时间调整影响

## 相关接口

- [je_clock_time_stamp](je_clock_time_stamp.md) - 获取当前时间戳
- [je_clock_sleep_for](je_clock_sleep_for.md) - 休眠指定时间
- [je_clock_sleep_until](je_clock_sleep_until.md) - 休眠直到指定时间
