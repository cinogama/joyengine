# je_ecs_universe_register_for_worlds_job

## 函数签名

```cpp
JE_API void je_ecs_universe_register_for_worlds_job(
    void* universe, const char* name, void(*job)(void*, void*), void* data);
```

## 描述

注册一个在每帧为每个 world 执行的任务。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| universe | void* | Universe 实例指针 |
| name | const char* | 任务名称 |
| job | void(*)(void*, void*) | 任务函数，参数为 (world, data) |
| data | void* | 传递给任务函数的用户数据 |

## 返回值

无

## 相关接口

- [je_ecs_universe_unregister_for_worlds_job](je_ecs_universe_unregister_for_worlds_job.md) - 注销任务
