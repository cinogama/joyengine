# je_ecs_world_entity_remove_component

## 函数签名

```c
JE_API void je_ecs_world_entity_remove_component(
    const jeecs::game_entity* entity,
    jeecs::typing::typeid_t type);
```

## 描述

从实体中移除一个组件。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `entity` | `const jeecs::game_entity*` | 指向实体索引的指针 |
| `type` | `jeecs::typing::typeid_t` | 组件类型的 ID |

## 返回值

无返回值。

## 用法

此函数用于在运行时从实体中移除组件。

### 示例

```cpp
jeecs::game_entity entity;
// ... 实体已创建并具有组件 ...

// 移除组件
const jeecs::typing::type_info* renderer_type = 
    je_typing_get_info_by_name("Renderer::Shape");

je_ecs_world_entity_remove_component(&entity, renderer_type->m_id);
```

## 注意事项

- 若实体索引无效或已失效，则最终无事发生
- 引擎对于增加或移除组件的操作遵循"最后生效"原则
- 如果生效的是移除组件操作：
  - 若实体已存在同类型组件，则移除之
  - 若实体不存在同类型组件，则无事发生
- 组件移除后，实体的 ArchType 会发生变化，旧的实体索引将失效

## 相关接口

- [je_ecs_world_entity_add_component](je_ecs_world_entity_add_component.md) - 添加组件
- [je_ecs_world_entity_get_component](je_ecs_world_entity_get_component.md) - 获取组件
