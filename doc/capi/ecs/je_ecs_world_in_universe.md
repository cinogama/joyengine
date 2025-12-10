# je_ecs_world_in_universe

## 函数签名

```cpp
JE_API void* je_ecs_world_in_universe(void* world);
```

## 描述

获取指定 World 所属的 Universe。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| world | void* | World 实例指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| void* | 所属的 Universe 实例指针 |

## 相关接口

- [je_ecs_world_create](je_ecs_world_create.md) - 创建 World
