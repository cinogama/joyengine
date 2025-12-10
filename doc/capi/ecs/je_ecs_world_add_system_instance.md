# je_ecs_world_add_system_instance

## 函数签名

```c
JE_API jeecs::game_system* je_ecs_world_add_system_instance(
    void* world,
    jeecs::typing::typeid_t type);
```

## 描述

向指定世界中添加一个指定类型的系统实例。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `void*` | 指向世界的指针 |
| `type` | `jeecs::typing::typeid_t` | 系统类型的 ID |

## 返回值

| 类型 | 描述 |
|------|------|
| `jeecs::game_system*` | 指向系统实例的指针；若添加失败返回 `nullptr` |

## 用法

此函数用于向世界中添加系统。系统负责处理特定组件的逻辑更新。

### 示例

```cpp
void* world = je_ecs_world_create(universe);

// 获取系统类型信息
const jeecs::typing::type_info* physics_system = 
    je_typing_get_info_by_name("Physics2D::World");

// 添加系统实例
jeecs::game_system* system_instance = 
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
- 若向一个正在销毁中的世界添加系统实例，返回 `nullptr`

## 相关接口

- [je_ecs_world_get_system_instance](je_ecs_world_get_system_instance.md) - 获取系统实例
- [je_ecs_world_remove_system_instance](je_ecs_world_remove_system_instance.md) - 移除系统实例
