# je_ecs_requirements_free

## 函数签名

```c
JE_API void je_ecs_requirements_free(je_collected_requirements_t* requirements);
```

## 描述

释放由 je_ecs_requirements_collect 生成的需求对象

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `requirements` | `je_collected_requirements_t*` | je_ecs_requirements_collect 返回的需求对象 |

## 返回值

| 类型 | 描述 |
|------|------|
| `void` | — |

## 相关接口

- [je_ecs_requirements_collect](je_ecs_requirements_collect.md) - 把一组组件需求整理为供切片查询使用的紧凑需求对象
