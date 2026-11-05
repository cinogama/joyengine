# je_log_callback_register

## 函数签名

```c
JE_API je_log_regid_t je_log_callback_register(je_log_callback_t callback, /* OPTIONAL */ void* userdata);
```

## 描述

用于注册一个日志发生时的回调函数，返回注册句柄。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `callback` | `je_log_callback_t` | — |
| `userdata` | `void*` | 可选。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_log_regid_t` | 见描述。 |
