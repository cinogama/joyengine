# je_ecs_universe_unregister_pre_for_worlds_job

## 函数签名

```cpp
JE_API void je_ecs_universe_unregister_pre_for_worlds_job(
    void* universe, const char* name);
```

## 描述

注销一个之前注册的前置世界任务。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| universe | void* | Universe 实例指针 |
| name | const char* | 任务名称 |

## 返回值

无

## 相关接口

- [je_ecs_universe_register_pre_for_worlds_job](je_ecs_universe_register_pre_for_worlds_job.md) - 注册任务
