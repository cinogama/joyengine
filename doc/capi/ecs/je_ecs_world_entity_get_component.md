# je_ecs_world_entity_get_component

## 函数签名

```c
JE_API void* je_ecs_world_entity_get_component(
    const jeecs::game_entity* entity,
    jeecs::typing::typeid_t type);
```

## 描述

从实体中获取一个组件的指针。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `entity` | `const jeecs::game_entity*` | 指向实体索引的指针 |
| `type` | `jeecs::typing::typeid_t` | 组件类型的 ID |

## 返回值

| 类型 | 描述 |
|------|------|
| `void*` | 指向组件实例的指针；若实体无效或不存在该组件返回 `nullptr` |

## 用法

此函数用于获取实体上的组件以读取或修改其数据。

### 示例

```cpp
jeecs::game_entity entity;
// ... 实体已创建 ...

// 获取组件
const jeecs::typing::type_info* transform_type = 
    je_typing_get_info_by_name("Transform::Translation");

Transform::Translation* transform = (Transform::Translation*)
    je_ecs_world_entity_get_component(&entity, transform_type->m_id);

if (transform != nullptr) {
    // 读取或修改组件数据
    transform->x += 1.0f;
}
```

## 注意事项

- 若实体索引无效或已失效，返回 `nullptr`
- 若实体不存在指定类型的组件，返回 `nullptr`
- 返回的指针在实体组件发生变化（添加/移除组件）前有效

## 相关接口

- [je_ecs_world_entity_add_component](je_ecs_world_entity_add_component.md) - 添加组件
- [je_ecs_world_entity_remove_component](je_ecs_world_entity_remove_component.md) - 移除组件
