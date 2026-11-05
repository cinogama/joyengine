# je_ecs_entity_name_set

## 函数签名

```cpp
JE_API const char* je_ecs_entity_name_set(
    jeecs::game_entity* entity, const char* name);
```

## 描述

设置实体的名称。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| entity | jeecs::game_entity* | 实体指针 |
| name | const char* | 要设置的名称 |

## 返回值

| 类型 | 描述 |
|------|------|
| const char* | 设置后的实体名称 |

## 相关接口

- [je_ecs_entity_name_get](je_ecs_entity_name_get.md) - 获取实体名称
