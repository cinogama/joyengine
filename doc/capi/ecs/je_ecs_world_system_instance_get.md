# je_ecs_world_system_instance_get

## 函数签名

```c
JE_API jeecs::game_system* je_ecs_world_system_instance_get(
    void* world,
    jeecs::typing::typeid_t type);
```

## 描述

从指定世界中获取一个指定类型的系统实例。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `void*` | 指向世界的指针 |
| `type` | `jeecs::typing::typeid_t` | 系统类型的 ID |

## 返回值

| 类型 | 描述 |
|------|------|
| `jeecs::game_system*` | 指向系统实例的指针；若世界中不存在此类型系统返回 `nullptr` |

## 用法

此函数用于获取世界中已添加的系统实例，通常用于访问系统的状态或配置。

### 示例

```cpp
// 获取物理系统实例
const jeecs::typing::type_info* physics_system_type = 
    je_typing_info_get_by_name("Physics2D::World");

jeecs::game_system* physics_system = 
    je_ecs_world_system_instance_get(world, physics_system_type->m_id);

if (physics_system != nullptr) {
    // 访问系统实例...
}
```

## 注意事项

- 如果指定类型的系统尚未添加到世界中，返回 `nullptr`
- 返回的指针在系统被移除前有效

## 相关接口

- [je_ecs_world_system_instance_add](je_ecs_world_system_instance_add.md) - 添加系统实例
- [je_ecs_world_system_instance_remove](je_ecs_world_system_instance_remove.md) - 移除系统实例
