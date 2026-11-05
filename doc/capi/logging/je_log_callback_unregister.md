# je_log_callback_unregister

## 函数签名

```c
JE_API /* OPTIONAL: If registered userdata was null */ void* je_log_callback_unregister(je_log_regid_t regid);
```

## 描述

用于释放（解除注册）一个之前注册的日志发生时回调函数。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `regid` | `je_log_regid_t` | — |

## 返回值

无返回值。
