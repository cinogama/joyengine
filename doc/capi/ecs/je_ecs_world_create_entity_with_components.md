# je_ecs_world_create_entity_with_components

## 函数签名

```c
JE_API void je_ecs_world_create_entity_with_components(
    je_GameWorld* world,
    je_GameEntity* out_entity,
    const je_TypeId* component_ids,
    size_t component_count);
```

## 描述

向指定世界中创建一个具有指定组件集合的实体。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_GameWorld*` | 指向世界的指针 |
| `out_entity` | `je_GameEntity*` | 用于接收创建结果的实体指针 |
| `component_ids` | `const je_TypeId*` | 组件类型 ID 数组 |
| `component_count` | `size_t` | 组件种类数量 |

## 返回值

无返回值。创建结果通过 `out_entity` 参数返回。

## 用法

此函数用于创建具有特定组件组合的实体。

### 示例

```cpp
// 获取组件类型
const je_TypeInfo* transform_type = 
    je_typing_get_info_by_name("Transform::Translation");
const je_TypeInfo* renderer_type = 
    je_typing_get_info_by_name("Renderer::Shape");

// 构建组件 ID 数组
const je_TypeId components[] = {
    transform_type->m_id,
    renderer_type->m_id,
};

// 创建实体
je_GameEntity entity;
je_ecs_world_create_entity_with_components(
    world, &entity, components, sizeof(components) / sizeof(je_TypeId));

// 获取并设置组件数据
auto* transform = (Transform::Translation*)
    je_ecs_world_entity_get_component(&entity, transform_type->m_id);
if (transform != nullptr) {
    transform->x = 0.0f;
    transform->y = 0.0f;
}
```

## 注意事项

- `component_ids` 应指向一个储存有 N 个类型 ID 的连续空间
- 其中 N 是组件种类数量（`component_count`）且不应该为 0
- 世界销毁是异步的（见 [je_ecs_world_destroy](je_ecs_world_destroy.md)）：向已提交销毁请求（正在销毁中）的世界创建实体仍会成功，`out_entity` 将被写入有效句柄，但该实体会在下一次世界更新时随世界一并销毁
- 向已被销毁的世界（指针已悬空）创建实体是未定义行为

## 相关接口

- [je_ecs_world_create_prefab_with_components](je_ecs_world_create_prefab_with_components.md) - 创建预设体
- [je_ecs_world_destroy_entity](je_ecs_world_destroy_entity.md) - 销毁实体
- [je_ecs_world_entity_add_component](je_ecs_world_entity_add_component.md) - 添加组件
