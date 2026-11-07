# je_log_callback_register

## 函数签名

```c
JE_API je_log_regid_t je_log_callback_register(je_log_callback_t callback, /* OPTIONAL */ void* userdata);
```

## 描述

用于注册一个日志发生时的回调函数，返回注册句柄。

回调在日志派发线程上按注册顺序被调用（引擎 bootup 前 / shutdown 后的日志在提交线程上直接派发）。`msg` 缓冲仅在回调返回前有效，需要保留请自行拷贝。解除注册与派发互斥：`je_log_callback_unregister` 返回后不会再有该回调被调用。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `callback` | `je_log_callback_t` | 日志回调，签名为 `void (*)(je_log_level_t level, const char* msg, void* userdata)`。 |
| `userdata` | `void*` | 可选。原样传给回调的用户数据。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_log_regid_t` | 注册句柄，用于 `je_log_callback_unregister` 解除注册。 |
