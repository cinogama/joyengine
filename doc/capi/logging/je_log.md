# je_log

## 函数签名

```c
JE_API void je_log(je_log_level_t level, const char* format, ...);
```

## 描述

以指定的日志等级，向日志系统提交一条日志。

消息文本会自动加上 `[时:分:秒.毫秒]` 时间戳前缀（如 `[14:23:05.128] 消息内容`），随后非阻塞入队，由日志派发线程依次调用已注册的回调并输出到控制台（Windows 下同时输出到调试器 OutputDebugString）。

引擎 bootup 之前 / shutdown 之后的日志会在调用线程上直接派发（回调 + 控制台输出），不会丢失。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `level` | `je_log_level_t` | 日志等级，见下方枚举说明。 |
| `format` | `const char*` | printf 风格格式串，后续可变参数为格式化实参。 |

## 返回值

无返回值。

## 日志等级 je_log_level_t

| 枚举值 | 值 | 含义 |
|--------|----|------|
| `JE_LOG_NORMAL` | 0 | 通常 |
| `JE_LOG_INFO` | 1 | 信息 |
| `JE_LOG_WARNING` | 2 | 警告 |
| `JE_LOG_ERROR` | 3 | 错误 |
| `JE_LOG_FATAL` | 4 | 致命错误 |
