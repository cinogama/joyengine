# je_ecs_world_create

## 函数签名

```c
JE_API void* je_ecs_world_create(void* in_universe);
```

## 描述

在指定的 Universe 中创建一个世界（World）。世界是组件和系统的集合。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `in_universe` | `void*` | 指向所属 Universe 的指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| `void*` | 指向新创建的世界的指针 |

## 用法

世界是引擎中实体和系统运行的容器。每个世界可以独立运行，在自己的工作线程中更新。

### 示例

```cpp
void* universe = je_ecs_universe_create();

// 创建世界
void* world = je_ecs_world_create(universe);

// 添加系统
const jeecs::typing::type_info* render_system = 
    je_typing_get_info_by_name("Renderer::System");
je_ecs_world_add_system_instance(world, render_system->m_id);

// 创建实体
jeecs::game_entity entity;
jeecs::typing::typeid_t components[] = {
    je_typing_get_info_by_name("Transform::Translation")->m_id,
    jeecs::typing::INVALID_TYPE_ID  // 结束标记
};
je_ecs_world_create_entity_with_components(world, &entity, components);

// 激活世界（开始更新循环）
je_ecs_world_set_able(world, true);
```

## 注意事项

- 世界在创建之后，默认为非激活状态
- 完成初始化操作后需要调用 `je_ecs_world_set_able(world, true)` 激活世界
- 每个世界有独立的更新循环

## 相关接口

- [je_ecs_world_destroy](je_ecs_world_destroy.md) - 销毁世界
- [je_ecs_world_set_able](je_ecs_world_set_able.md) - 设置世界激活状态
- [je_ecs_world_add_system_instance](je_ecs_world_add_system_instance.md) - 添加系统
- [je_ecs_world_create_entity_with_components](je_ecs_world_create_entity_with_components.md) - 创建实体
