# je_ecs_world_query_slice_dependence

## 函数签名

```cpp
JE_API bool je_ecs_world_query_slice_dependence(
    void* world,
    jeecs::selector* selector,
    jeecs::dependence* dependence);
```

## 描述

查询 World 中满足依赖条件的实体切片。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| world | void* | World 实例指针 |
| selector | jeecs::selector* | 选择器 |
| dependence | jeecs::dependence* | 依赖条件 |

## 返回值

| 类型 | 描述 |
|------|------|
| bool | 是否找到满足条件的实体 |

## 相关接口

- [je_ecs_world_update_dependences_archinfo](je_ecs_world_update_dependences_archinfo.md) - 更新依赖的原型信息
