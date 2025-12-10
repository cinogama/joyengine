# je_clock_sleep_until

## 函数签名

```c
JE_API void je_clock_sleep_until(double time);
```

## 描述

挂起当前线程直到指定的引擎时间点。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `time` | `double` | 目标时间点（引擎运行时间，秒） |

## 返回值

无返回值。

## 用法

此函数用于精确控制线程唤醒时间，常用于帧率控制。

### 示例

```cpp
// 目标帧率 60 FPS
double frame_time = 1.0 / 60.0;
double next_frame = je_clock_time() + frame_time;

while (running) {
    // 处理游戏逻辑...
    
    // 等待到下一帧时间
    je_clock_sleep_until(next_frame);
    next_frame += frame_time;
}
```

## 注意事项

- 参数 `time` 是引擎运行时间，应与 `je_clock_time()` 返回值配合使用
- 如果指定时间已过去，函数会立即返回
- 实际唤醒时间可能略有误差

## 相关接口

- [je_clock_time](je_clock_time.md) - 获取引擎运行时间
- [je_clock_sleep_for](je_clock_sleep_for.md) - 休眠指定时长
