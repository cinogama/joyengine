# 预制体（Prefab）

本文档是[术语表](README.md)中「预制体」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述预制体的行为约定、创建与实例化；
* **第二部分·底层实现**——面向引擎维护者，描述上述约定在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 预制体是什么

预制体（Prefab）是作为模板的特殊[实体](entity.md)：它和普通实体一样由一组组件构成、存放在世界中，依照它可以*实例化*出组件构成相同、组件值也相同的新实体。引擎源码与编辑器中也把它称作“预设体”。

与普通实体相比，预制体有三条行为约定：

* **不会被任何切片选中**——系统看不到它，它不参与游戏逻辑，也不会被渲染；
* **不能增删组件**——组件构成在创建时就固定了，增删请求会被拒绝并报错；但组件的**成员值可以随时修改**；
* **句柄是持久的**——可以跨帧保存、反复使用，而普通实体的句柄只保证在当帧有效（见[实体·句柄的有效期](entity.md#句柄的有效期)）。

### 创建预制体

以一组组件类型创建预制体（Woolang：`world::add_prefab(w, types)`；C++：`world.add_prefab<CompTs...>()`）。与创建实体一样，调用返回时组件已经构造完毕，可以立即写入模板值（示例中的 `Health` 是[组件](component.md)一文中声明的脚本组件）：

```woolang
let prefab = w->add_prefab([
    Transform::LocalPosition::type::typeinfo,
    Transform::Translation::type::typeinfo,
    Health::type::typeinfo,
])->unwrap;

match (prefab->towoo::get_component:<Health>())
{
    value(h)? h.hp->set(100);   // 设置模板值
    none?;
}
```

### 实例化

依照预制体创建新实体（Woolang：`world::instance(w, prefab)`；C++：`world.add_entity(prefab)`）：

* **拷贝发生在调用的那一刻**：新实体的每个组件都从预制体当前的组件值拷贝而来；
* 新实体是一个**普通实体**：下一帧开始生效、可被切片选中，句柄同样只在当帧有效；
* 实例与预制体从此互不影响：之后修改预制体的组件值，不会波及已经实例化出的实体；
* 组件的“拷贝”遵循该组件类型自己的拷贝语义。例如 `Transform::Anchor` 被拷贝时会生成新的 uuid，因此实例之间不会共享父子层级中的身份；
* 传入失效的预制体句柄时，引擎记录错误日志，并返回一个无效的实体句柄。

```cpp
using namespace jeecs;

game_entity prefab = world.add_prefab<
    Transform::LocalPosition, Transform::Translation, MyGame::Health>();
prefab.get_component<MyGame::Health>()->hp = 100;   // 当帧即可写入模板值

// 之后的任意一帧：
game_entity e = world.add_entity(prefab);          // 依照模板实例化
```

严格来说，任何有效的实体都可以作为实例化的源（相当于克隆它），但普通实体的句柄只在当帧有效；需要长期保存、反复使用的模板，就应当创建为预制体。

### 销毁预制体

预制体的销毁与普通实体相同（Woolang：`prefab->close()`；C++：`prefab.close()` 或 `world.remove_entity(prefab)`），下一帧生效，此后它的句柄失效。已经实例化出的实体不受影响。世界被销毁时，其中的预制体随之销毁。

### 典型用途

* **批量生成**：子弹、敌人、掉落物等需要反复生成的对象，在初始化时创建好预制体并保存句柄（例如存进脚本系统的上下文或 C++ 系统的成员），之后按需实例化；
* **从实体文件加载模板**：内置的游戏运行库提供 `JEGaming::Loading::Entity::loadPrefab(w, path)`（`builtin/gaming/Loading/Entity.wo`），把实体文件（`.je4entity`）还原为预制体：

```woolang
import builtin::gaming::Loading::Entity;

match (JEGaming::Loading::Entity::loadPrefab(w, "@/prefabs/bullet.je4entity"))
{
    ok(prefab)?
        do w->instance(prefab);
    err(msg)?
        je::logerr(msg);
}
```

### 编辑器中的“预设体”资源

编辑器里的“预设体”是一个**资源层面**的概念，与上文运行时的预制体不是一回事：

* 预设体资源是一个 `.je4entity` 实体文件。在实体列表中把实体拖到资源管理器即可保存为预设体，该实体随即与文件建立关联（关联的实体在实体列表中以蓝色显示）；
* 把 `.je4entity` 文件拖进世界视口会创建一个关联该文件的实例——它是一个**普通实体**，关联关系记录在其 `Editor::Prefab` 组件的 `path` 成员中；
* 保存关联实体时只记录它与预设体文件的差异；预设体文件本身也可以是另一个预设体的变体；
* 检视面板会高亮与预设体不一致的成员；预设体文件更新后，编辑器把新值同步到各个实例，实例上被改写过的成员保持不变；
* 编辑器的实体复制/粘贴基于序列化快照：复制时保存实体数据，粘贴时据此创建新实体，并不经过运行时预制体。

两者的衔接点就是上面的 `loadPrefab`：编辑器产出的 `.je4entity` 文件，在游戏运行时可以被加载为真正的预制体。

### API 速览

#### Woolang（`je::ecs`）

| 函数 | 说明 |
| --- | --- |
| `world::add_prefab(w, types)` | 以组件类型数组创建预制体，返回 `option<entity>`（数组为空时为 `none`） |
| `world::instance(w, prefab)` | 依照预制体（或任意有效实体）实例化新实体 |
| `prefab->close()` | 销毁预制体（下一帧生效） |
| `JEGaming::Loading::Entity::loadPrefab(w, path)` | 从实体文件加载预制体，返回 `result<entity, string>` |

#### C++（`jeecs::game_world`，头文件 `jeecs/game.hpp`）

| 方法 | 说明 |
| --- | --- |
| `world.add_prefab<CompTs...>()` | 以组件类型集合创建预制体 |
| `world.add_entity(prefab)` | 依照预制体（或任意有效实体）实例化新实体 |
| `world.remove_entity(prefab)` / `prefab.close()` | 销毁预制体（下一帧生效） |

### 相关术语

参见[术语表](README.md)：

* [实体](entity.md)——预制体本质上是一种特殊状态的实体；
* [组件](component.md)——实例化时被拷贝的数据；
* [切片](slice.md)——预制体不会被切片选中；
* [编辑器](editor.md)——预设体资源的保存、实例化与同步。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。实现主体位于 `src/jeecs_basic_arch_system.cpp`（`World::create_entity_with_prefab`、`ArchType::instance_entity`、`CommandBuffer::update`），C ABI 与行为注释见 `include/jeecs/ecs.hpp`。

### 预制体就是 PREFAB 状态的实体

`je_ecs_world_prefab_create_with_components` 与创建普通实体走同一条路径，只是目标状态不同：`World::create_entity_with_component(types, JE_ENTITY_STAT_PREFAB)`。预制体与组件构成相同的普通实体**位于同一个原型、同一批 chunk 中**，没有独立的存储；区别仅在于槽位元信息中的状态是 `JE_ENTITY_STAT_PREFAB`。与新实体一样，这个状态在下一帧命令缓冲更新时才写入，此前槽位处于 `UNAVAILABLE`，但组件已经就地构造，句柄的版本校验也已成立。

### 三条约定的实现

* **切片不可见**：C++ 切片、towoo 系统与 `entity_selector` 遍历时只接受 `JE_ENTITY_STAT_READY` 的槽位；编辑器枚举世界实体用的 `je_debug_world_entities_get_all` 同样只收集 `READY`，因此预制体也不会出现在编辑器的实体列表里。
* **不能增删组件**：`CommandBuffer::update` 先应用实体的目标状态，再检查状态是否为 `PREFAB`；若是，则为每个增删请求记录错误日志（`Cannot add component ... to prefab` / `Cannot remove component ... from prefab`），并析构释放临时组件实例。由于目标状态先于检查应用，**在创建预制体的同一帧提交的增删请求同样会被拒绝**。
* **句柄持久**：预制体的组件构成不会变化，因此永远不会换原型；碎片整理只搬运 `READY` 的实体，`PREFAB` 槽位原地不动（源码注释：预设体与尚未提交的实体“必须保证实体的数据保持在原地”）。槽位版本不变，句柄也就一直有效。

⚠️ 注意 含有预制体的 chunk 永远不会被碎片整理搬空，因而也永远不会被释放——即便其中其余实体都已被搬走。源码中留有 TODO：“将包含预设体的区块移动到前面，这样可以最大化利用空间”。在实体频繁增删的原型里大量散布预制体，会削弱碎片整理的效果。

### 实例化路径

`je_ecs_world_entity_create_with_prefab` 调用 `World::create_entity_with_prefab`：

1. 以 `is_valid()` 校验源句柄（只比较版本号，不检查状态）；失效则记录错误日志 “It's not allowed to create entity with invalid prefab entity, very dangerous.”，返回 chunk 指针为空的句柄——对它的后续操作都会被忽略；
2. 直接取源实体所在的原型调用 `instance_entity(prefab)`：在**同一个原型**中分配槽位，对每个组件类型以 `typing::copy` 从源槽位拷贝构造——无需计算类型集合，也无需查找原型；
3. `CommandBuffer::init_new_entity(entity, JE_ENTITY_STAT_READY)`，下一帧生效。

因为只校验版本，源可以是 `READY` 的普通实体、`PREFAB` 的预制体，甚至是本帧刚创建、尚未生效的实体——与 C ABI 注释所说“原件实体可以由三种创建接口中的任意一种创建”一致。拷贝在调用时完成，读取的是源实体此刻的组件值。

拷贝语义来自类型信息中的拷贝函数：C++ 组件即拷贝构造函数（`Transform::Anchor` 的拷贝构造会重新生成 uid，`Editor::EntityId` 的拷贝构造会分配新的调试 ID）；脚本组件由 `ToWooBaseComponent` 逐成员委托给成员类型的拷贝函数。

⚠️ 注意 若组件类型不可拷贝构造，`typing::default_functions<T>::copier` 只会记录 “not copy-constructible” 错误日志，实例中该组件的内存**保持未构造状态**，之后对它的访问与析构都是未定义行为。用作预制体的组件必须可拷贝构造。

### 销毁流程

预制体的销毁与普通实体完全一致：`remove_entity` 设置销毁标记，下一帧 `CommandBuffer::update` 析构全部组件并 `command_close_entity`（状态置为 `UNAVAILABLE`、版本递增、槽位归还），持久句柄自此失效。世界销毁时，`close_all_entity` 遍历所有被占用的槽位（不在空闲栈中的槽位），预制体也在其中。

### 运行时库与编辑器中的用法

* `JEGaming::Loading::Entity::loadPrefab` 读取文件并解析为 TOML，交给 `JEGaming::Saving::Entity::restorePrefab`，后者以 `world::add_prefab` 为创建器调用通用的 `_restore`：先按数据中的组件列表创建预制体，再逐个 `get_component` 写入成员值，最后绑定纹理、设置形状与着色器。这依赖“组件在创建时已就地构造”这一事实。注意 `loadPrefab` 直接解析文件原始内容，**不展开预设体的变体链**（展开由 `JEGaming::Saving::Entity::load` 负责）。
* 编辑器的预设体资源不使用 `PREFAB` 状态：实例是普通实体，关联信息存于 `Editor::Prefab` 组件（`src/jeecs_core_editor_components.hpp`，唯一成员为 `path`），差异保存、变体链与实例同步全部在 Woolang 层完成（`builtin/editor/Editor/Saving/Entity.wo`、`PrefabSync.wo`）。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 预制体创建、实例化 C ABI 与行为注释 | `include/jeecs/ecs.hpp` |
| `create_entity_with_prefab` / `instance_entity` / 预制体增删拒绝 / 碎片整理 | `src/jeecs_basic_arch_system.cpp` |
| C++ 包装（`add_prefab` / `add_entity(prefab)`） | `include/jeecs/game.hpp` |
| Woolang 绑定 | `je/ecs.wo`；原生侧 `src/jeecs_ecs_woolang_api.cpp` |
| 从实体文件加载预制体 | `builtin/gaming/Loading/Entity.wo`、`builtin/gaming/Saving/Entity.wo` |
| 编辑器预设体资源（保存、实例化、同步） | `builtin/editor/Editor/Saving/Entity.wo`、`PrefabSync.wo`、`WorldEditorViewport/WindowLifecycle.wo` |
