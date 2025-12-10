# je_ecs_world_destroy_entity

## 函数签名

```c
JE_API void je_ecs_world_destroy_entity(
    void* world,
    const jeecs::game_entity* entity);
```

## 描述

从世界中销毁一个实体索引指定的实体及其所有组件。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `void*` | 指向世界的指针 |
| `entity` | `const jeecs::game_entity*` | 指向需要销毁的实体索引的指针 |

## 返回值

无返回值。

## 用法

此函数用于销毁不再需要的实体。

### 示例

```cpp
jeecs::game_entity entity;
// ... 创建实体 ...

// 销毁实体
je_ecs_world_destroy_entity(world, &entity);
```

## 注意事项

- 若实体索引是无效值或已失效，则无事发生
- 销毁操作会在合适的时机生效（可能不是立即）
- 销毁后，该实体的所有组件都会被析构和释放

## 相关接口

- [je_ecs_world_create_entity_with_components](je_ecs_world_create_entity_with_components.md) - 创建实体
