# je_log_callback_unregister

## 函数签名

```c
JE_API /* OPTIONAL: If registered userdata was null */ void* je_log_callback_unregister(je_log_regid_t regid);
```

## 描述

用于释放（解除注册）一个之前注册的日志发生时回调函数，返回该回调注册时的 userdata。

解除注册与日志派发互斥：本函数返回后不会再有该回调被调用。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `regid` | `je_log_regid_t` | `je_log_callback_register` 返回的注册句柄。 |

## 返回值

| 类型 | 描述 |
|------|------|
| `void*` | 该回调注册时传入的 userdata；注册时为 null 则返回 null。 |
