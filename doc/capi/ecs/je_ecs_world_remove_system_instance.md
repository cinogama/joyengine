# je_ecs_world_remove_system_instance

## 函数签名

```c
JE_API void je_ecs_world_remove_system_instance(
    void* world,
    jeecs::typing::typeid_t type);
```

## 描述

从指定世界中移除一个指定类型的系统实例。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `void*` | 指向世界的指针 |
| `type` | `jeecs::typing::typeid_t` | 系统类型的 ID |

## 返回值

无返回值。

## 用法

此函数用于从世界中移除系统。

### 示例

```cpp
// 移除渲染系统
const jeecs::typing::type_info* render_system_type = 
    je_typing_get_info_by_name("Renderer::System");

je_ecs_world_remove_system_instance(world, render_system_type->m_id);
```

## 注意事项

- 每次更新时，一帧内"最后"执行的操作将会生效
- 如果生效的是移除系统操作：
  - 若此前世界中存在同类型系统，则移除
  - 若此前世界中不存在同类型系统，则无事发生

## 相关接口

- [je_ecs_world_add_system_instance](je_ecs_world_add_system_instance.md) - 添加系统实例
- [je_ecs_world_get_system_instance](je_ecs_world_get_system_instance.md) - 获取系统实例
