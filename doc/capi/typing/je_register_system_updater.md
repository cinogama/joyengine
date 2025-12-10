# je_register_system_updater

## 函数签名

```c
JE_API void je_register_system_updater(
    const jeecs::typing::type_info* _type,
    jeecs::typing::on_enable_or_disable_func_t _on_enable,
    jeecs::typing::on_enable_or_disable_func_t _on_disable,
    jeecs::typing::update_func_t _pre_update,
    jeecs::typing::update_func_t _state_update,
    jeecs::typing::update_func_t _update,
    jeecs::typing::update_func_t _physics_update,
    jeecs::typing::update_func_t _transform_update,
    jeecs::typing::update_func_t _late_update,
    jeecs::typing::update_func_t _commit_update,
    jeecs::typing::update_func_t _graphic_update);
```

## 描述

向引擎的类型管理器注册指定系统类型的更新方法。此函数用于定义系统在各个更新阶段的行为。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `_type` | `const jeecs::typing::type_info*` | 系统类型信息指针 |
| `_on_enable` | `on_enable_or_disable_func_t` | 系统激活时的回调 |
| `_on_disable` | `on_enable_or_disable_func_t` | 系统失活时的回调 |
| `_pre_update` | `update_func_t` | 预更新阶段回调 |
| `_state_update` | `update_func_t` | 状态更新阶段回调 |
| `_update` | `update_func_t` | 主更新阶段回调 |
| `_physics_update` | `update_func_t` | 物理更新阶段回调 |
| `_transform_update` | `update_func_t` | 变换更新阶段回调 |
| `_late_update` | `update_func_t` | 延迟更新阶段回调 |
| `_commit_update` | `update_func_t` | 提交更新阶段回调 |
| `_graphic_update` | `update_func_t` | 图形更新阶段回调 |

### 函数指针类型

```cpp
using on_enable_or_disable_func_t = void (*)(void* system_instance);
using update_func_t = void (*)(void* system_instance);
```

### 更新阶段说明

| 阶段 | 描述 |
|------|------|
| `OnEnable` | 系统激活时调用，用于初始化 |
| `OnDisable` | 系统失活时调用，用于清理 |
| `PreUpdate` | 用户预更新 |
| `StateUpdate` | 将初始状态给予各组件 (Animation, VirtualGamepadInput) |
| `Update` | 用户主更新 |
| `PhysicsUpdate` | 物理引擎状态更新 |
| `TransformUpdate` | 物体变换和关系更新 (Transform) |
| `LateUpdate` | 用户延迟更新 |
| `CommitUpdate` | 提交最终生效的数据 (Transform, Audio, ScriptRuntime) |
| `GraphicUpdate` | 将数据呈现到用户界面 |

## 返回值

无返回值。

## 用法

此函数用于注册系统的各个更新回调，使得系统能够在正确的时机执行逻辑。

### 示例

```cpp
class MySystem {
public:
    void OnEnable() { /* 初始化 */ }
    void OnDisable() { /* 清理 */ }
    void Update() { /* 每帧更新逻辑 */ }
};

// 包装函数
void my_system_on_enable(void* self) { ((MySystem*)self)->OnEnable(); }
void my_system_on_disable(void* self) { ((MySystem*)self)->OnDisable(); }
void my_system_update(void* self) { ((MySystem*)self)->Update(); }

// 注册系统更新方法
const jeecs::typing::type_info* system_type = /* 已注册的系统类型 */;
je_register_system_updater(
    system_type,
    my_system_on_enable,   // OnEnable
    my_system_on_disable,  // OnDisable
    nullptr,               // PreUpdate
    nullptr,               // StateUpdate
    my_system_update,      // Update
    nullptr,               // PhysicsUpdate
    nullptr,               // TransformUpdate
    nullptr,               // LateUpdate
    nullptr,               // CommitUpdate
    nullptr                // GraphicUpdate
);
```

## 注意事项

- 使用本地 typeinfo，而非全局通用 typeinfo
- 不需要的更新阶段可以传入 `nullptr`
- 更新函数应当尽快返回，避免阻塞整个更新循环

## 相关接口

- [je_typing_register](je_typing_register.md) - 注册类型
- [je_ecs_world_add_system_instance](../ecs/je_ecs_world_add_system_instance.md) - 向世界添加系统实例
