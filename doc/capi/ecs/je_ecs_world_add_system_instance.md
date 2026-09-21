# je_ecs_world_add_system_instance

## 函数签名

```c
JE_API je_System je_ecs_world_add_system_instance(
    je_GameWorld* world,
    je_TypeId type);
```

## 描述

向指定世界中添加一个指定类型的系统实例。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_GameWorld*` | 指向世界的指针 |
| `type` | `je_TypeId` | 系统类型的 ID |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_System` | 指向系统实例的指针；若给定的类型不是一个系统类型，返回 `nullptr` |

## 用法

此函数用于向世界中添加系统。系统负责处理特定组件的逻辑更新。

### 示例

```cpp
je_GameUniverse* universe = je_ecs_universe_create();
je_GameWorld* world = je_ecs_world_create(universe);

// 获取系统类型信息
const je_TypeInfo* physics_system = 
    je_typing_get_info_by_name("Physics2D::World");

// 添加系统实例
je_System system_instance = 
    je_ecs_world_add_system_instance(world, physics_system->m_id);

if (system_instance != nullptr) {
    // 系统添加成功
}
```

## 注意事项

- 每次更新时，一帧内"最后"执行的操作将会生效
- 如果生效的是添加系统操作：
  - 若此前世界中不存在同类型系统，则添加
  - 若此前世界中已存在同类型系统，则替换
- 向正在销毁中的世界添加系统实例仍会返回有效实例，但该系统会在下一次世界更新时随世界一并销毁，永远不会执行

## 相关接口

- [je_ecs_world_get_system_instance](je_ecs_world_get_system_instance.md) - 获取系统实例
- [je_ecs_world_remove_system_instance](je_ecs_world_remove_system_instance.md) - 移除系统实例
