# je_ecs_get_name_of_entity

## 函数签名

```cpp
JE_API const char* je_ecs_get_name_of_entity(const jeecs::game_entity* entity);
```

## 描述

获取实体的名称。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| entity | const jeecs::game_entity* | 实体指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| const char* | 实体名称，如果未设置则返回 nullptr |

## 相关接口

- [je_ecs_set_name_of_entity](je_ecs_set_name_of_entity.md) - 设置实体名称
