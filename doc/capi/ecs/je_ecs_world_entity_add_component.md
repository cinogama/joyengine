# je_ecs_world_entity_add_component

## 函数签名

```c
JE_API void* je_ecs_world_entity_add_component(
    const jeecs::game_entity* entity,
    jeecs::typing::typeid_t type);
```

## 描述

向实体中添加一个组件。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `entity` | `const jeecs::game_entity*` | 指向实体索引的指针 |
| `type` | `jeecs::typing::typeid_t` | 组件类型的 ID |

## 返回值

| 类型 | 描述 |
|------|------|
| `void*` | 指向新组件实例的指针；若实体无效返回 `nullptr` |

## 用法

此函数用于在运行时向实体添加新组件。

### 示例

```cpp
jeecs::game_entity entity;
// ... 创建实体 ...

// 添加新组件
const jeecs::typing::type_info* velocity_type = 
    je_typing_get_info_by_name("Physics2D::Velocity");

void* velocity = je_ecs_world_entity_add_component(&entity, velocity_type->m_id);
if (velocity != nullptr) {
    // 设置组件数据...
}
```

## 注意事项

- 无论实体是否已存在指定组件，总是创建一个新的组件实例并返回其地址
- 引擎对于增加或移除组件的操作遵循"最后生效"原则
- 如果生效的是添加操作：
  - 若实体已有存在的组件，则替换之
  - 若实体不存在此组件，则更新实体，旧的实体索引将失效
- 若实体索引无效或已失效，返回 `nullptr`

## 相关接口

- [je_ecs_world_entity_remove_component](je_ecs_world_entity_remove_component.md) - 移除组件
- [je_ecs_world_entity_get_component](je_ecs_world_entity_get_component.md) - 获取组件
