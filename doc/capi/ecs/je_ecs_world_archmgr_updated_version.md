# je_ecs_world_archmgr_updated_version

## 函数签名

```cpp
JE_API size_t je_ecs_world_archmgr_updated_version(void* world);
```

## 描述

获取 World 的原型管理器更新版本号。当 World 中的实体组件发生变化时，版本号会增加。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| world | void* | World 实例指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| size_t | 原型管理器版本号 |

## 相关接口

- [je_ecs_world_update_dependences_archinfo](je_ecs_world_update_dependences_archinfo.md) - 更新依赖的原型信息
