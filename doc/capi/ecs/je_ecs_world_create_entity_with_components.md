# je_ecs_world_create_entity_with_components

## 函数签名

```c
JE_API void je_ecs_world_create_entity_with_components(
    void* world,
    jeecs::game_entity* out_entity,
    const jeecs::typing::typeid_t* component_ids);
```

## 描述

向指定世界中创建一个具有指定组件集合的实体。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `void*` | 指向世界的指针 |
| `out_entity` | `jeecs::game_entity*` | 用于接收创建结果的实体指针 |
| `component_ids` | `const jeecs::typing::typeid_t*` | 组件类型 ID 数组，以 `INVALID_TYPE_ID` 结尾 |

## 返回值

无返回值。创建结果通过 `out_entity` 参数返回。

## 用法

此函数用于创建具有特定组件组合的实体。

### 示例

```cpp
// 获取组件类型
const jeecs::typing::type_info* transform_type = 
    je_typing_get_info_by_name("Transform::Translation");
const jeecs::typing::type_info* renderer_type = 
    je_typing_get_info_by_name("Renderer::Shape");

// 构建组件 ID 数组（以 INVALID_TYPE_ID 结尾）
jeecs::typing::typeid_t components[] = {
    transform_type->m_id,
    renderer_type->m_id,
    jeecs::typing::INVALID_TYPE_ID  // 结束标记
};

// 创建实体
jeecs::game_entity entity;
je_ecs_world_create_entity_with_components(world, &entity, components);

// 获取并设置组件数据
auto* transform = (Transform::Translation*)
    je_ecs_world_entity_get_component(&entity, transform_type->m_id);
if (transform != nullptr) {
    transform->x = 0.0f;
    transform->y = 0.0f;
}
```

## 注意事项

- `component_ids` 应指向一个储存有 N+1 个类型 ID 的连续空间
- 其中 N 是组件种类数量且不应该为 0
- 空间的最后必须是 `jeecs::typing::INVALID_TYPE_ID` 作为结束标记
- 若向一个正在销毁中的世界创建实体，创建失败，`out_entity` 将被写入无效值

## 相关接口

- [je_ecs_world_create_prefab_with_components](je_ecs_world_create_prefab_with_components.md) - 创建预设体
- [je_ecs_world_destroy_entity](je_ecs_world_destroy_entity.md) - 销毁实体
- [je_ecs_world_entity_add_component](je_ecs_world_entity_add_component.md) - 添加组件
