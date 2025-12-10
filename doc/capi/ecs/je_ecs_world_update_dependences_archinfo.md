# je_ecs_world_update_dependences_archinfo

## 函数签名

```cpp
JE_API void je_ecs_world_update_dependences_archinfo(
    void* world,
    jeecs::dependence* dependences,
    size_t dependences_count);
```

## 描述

更新 World 中依赖组件的原型信息。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| world | void* | World 实例指针 |
| dependences | jeecs::dependence* | 依赖数组 |
| dependences_count | size_t | 依赖数量 |

## 返回值

无

## 相关接口

- [je_ecs_world_archmgr_updated_version](je_ecs_world_archmgr_updated_version.md) - 获取原型管理器版本号
