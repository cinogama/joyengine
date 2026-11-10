# 实体（Entity）

本文档是[术语表](README.md)中「实体」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述实体的本质、增删与句柄的使用约定；
* **第二部分·底层实现**——面向引擎维护者，描述实体句柄、状态与结构变更在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 实体是什么

实体（Entity）在 JoyEngine 中是一个“不存在”的概念：它没有自己的 ID、没有类型，也没有名字字段——实体就是若干*组件*实例构成的集合，或者说，**实体只是这些组件之间的集合关系本身**。文档中提到“实体”时，其同义词是 `组件集合`。

由此带来几个直接的推论：

* 实体“是什么”完全由它拥有哪些组件决定，例如带有摄像机组件的实体就是[摄像机](camera.md)；
* 给实体增删组件，就是把它变成另一种组件构成的实体；
* 系统并不“持有”实体，而是每帧通过[切片](slice.md)重新找到符合条件的实体。

代码中通过*实体句柄*引用实体：Woolang 侧是 `je::entity`，C++ 侧是 `jeecs::game_entity`。句柄只记录“去哪里找这组组件”，并不是实体本身。

### 创建与销毁

#### 创建

在世界中以一组组件类型创建实体（Woolang：`world::add_entity(w, types)`；C++：`world.add_entity<CompTs...>()`）。组件列表不能为空，Woolang 侧传入空数组会得到 `option::none`。

创建调用返回时，组件实例**已经在世界的存储中构造完毕**（取各组件的默认值），可以立即用 `get_component` 取出并写入初值；但实体要到**下一帧开始时才正式生效**，在此之前不会出现在任何切片中，系统也就看不到它。

```woolang
let e = w->add_entity([
    Transform::LocalPosition::type::typeinfo,
    Transform::Translation::type::typeinfo,
])->unwrap;

// 当帧即可写入初值
match (e->towoo::get_component:<Transform::LocalPosition>())
{
    value(p)? p.pos->set((0., 1., 0.) as vec3);
    none?;
}
```

#### 销毁

`e->close()`（C++：`e.close()` 或 `world.remove_entity(e)`）销毁实体。销毁同样是**异步**的：调用只是提交请求，实体在下一帧开始时连同全部组件一起被销毁，**本帧剩余时间里它依然存在**，仍会出现在切片中。对已失效的句柄调用销毁则无事发生。世界被销毁时，其中的实体随之全部销毁。

### 增删组件

增删组件（Woolang：`e->towoo::add_component:<T>()` / `e->towoo::remove_component:<T>()`，或以类型信息为参数的 `add_component(type)` / `remove_component(type)`；C++：`e.add_component<T>()` / `e.remove_component<T>()`）是结构变更，经命令缓冲在**下一帧开始时**统一生效（见[世界·结构变更的生效时机](world.md#结构变更的生效时机)）：

* 同一帧内对**同一组件类型**的多次操作，**最后一次生效**；
* 添加实体已有的组件，等于用新实例**替换**旧实例；移除实体没有的组件，无事发生；
* 组件构成真正发生变化后，实体会被搬到另一处存储，旧句柄随之失效。

`add_component` 会立即返回一个新的组件实例，但它**还没有进入实体**——这是命令缓冲中的临时实例，下一帧才会被原样搬进实体。因此：

* 新组件的初值应当**直接写在 `add_component` 返回的实例上**；
* 当帧内 `get_component` 查不到这个新组件（它只查实体当前已有的组件，若已有同类型旧组件，查到的是旧的）；
* 返回的实例在下一帧搬运完成后即被释放，**不要跨帧保存**。

以[组件](component.md)一文中声明的 `Health` 为例：

```woolang
let h = e->towoo::add_component:<Health>();
h.hp->set(100);    // 写在临时实例上，下一帧随实例一起进入实体
```

```cpp
auto* h = e.add_component<MyGame::Health>();
h->hp = 100;       // 同理
```

### 句柄的有效期

**非预制体的实体句柄只保证在当前这一帧内有效**，任何情况下都不应跨帧保存。原因有二：实体的组件构成变化时会被搬到另一处存储；引擎在每帧开始时还会做碎片整理，把实体搬到更紧凑的位置。两种搬动都会使旧句柄失效。

失效句柄的行为：

* 所在世界仍存在时，对失效句柄的操作通常不生效、`get_component` 返回空——但只是“通常”：跨帧持有的句柄可能指向已被释放的存储，后果是未定义行为（见[第二部分·碎片整理与悬垂句柄](#碎片整理与悬垂句柄)）；
* 所在世界已被销毁时，对句柄的任何操作都可能直接崩溃，**不要跨世界保存句柄**。

需要跨帧“记住”某个实体时，正确的做法是：

1. **每帧通过切片重新找到它**——系统本来就是这样工作的；
2. 需要让一个实体“指向”另一个实体时，在组件里保存一个稳定的标识，再在切片中按标识匹配。引擎的父子层级就是这样实现的：父实体带有 `Transform::Anchor`（内含一个 uuid），子实体的 `Transform::LocalToParent` 记录父实体的 uuid（见[变换](transform.md)）；
3. 需要长期持有、反复使用的模板实体，使用[预制体](prefab.md)——预制体的句柄是持久的。

句柄可以用 `==` / `!=` 比较，比较的是句柄的完整内容（存储位置与版本），因此同一实体被搬动前后的两个句柄并不相等。

### 实体名

实体名不是实体的固有属性，而是保存在内置组件 `Editor::Name` 中，主要服务于编辑器与调试，**不建议在游戏逻辑中依赖实体名**。

* Woolang 侧通过 `je::internal` 模块中的 `e->internal::name()` / `e->internal::set_name(name)` 读写；C++ 包装类 `game_entity` 没有提供名字相关的方法；
* 实体已有 `Editor::Name` 时，设置立即生效；没有时会先为它添加该组件——这是一次结构变更：新名字下一帧才读得到，当前句柄也会在下一帧失效。

### API 速览

#### Woolang（`je::ecs` / `je::towoo`）

| 接口 | 说明 |
| --- | --- |
| `world::add_entity(w, types)` | 以组件类型数组创建实体，返回 `option<entity>` |
| `e->close()` | 请求销毁实体（下一帧生效） |
| `e->get_world()` | 取实体所在的世界 |
| `e->towoo::get_component:<T>()` | 按类型取组件，返回 `option<T>` |
| `e->towoo::add_component:<T>()` / `remove_component:<T>()` | 按类型增删组件 |
| `e->get_component(type)` / `add_component(type)` / `remove_component(type)` | 以类型信息访问、增删组件（得到无类型的 `component` 结构） |
| `e->get_shape()` / `set_shape()` / `get_shaders()` / `set_shaders()` / `get_textures()` / `bind_texture()` | 读写渲染相关组件的便捷函数（实体须已带对应组件，否则不生效） |
| `e->chunkinfo()` | 句柄内容的字符串形式，调试用 |
| `e->internal::name()` / `set_name(name)` | 读写实体名（`je::internal`） |

#### C++（`jeecs::game_entity` / `game_world`，头文件 `jeecs/game.hpp`）

| 方法 | 说明 |
| --- | --- |
| `world.add_entity<CompTs...>()` | 以组件类型集合创建实体 |
| `world.remove_entity(e)` / `e.close()` | 请求销毁实体（下一帧生效） |
| `e.get_component<T>()` | 取组件指针，无此组件或句柄失效时为 `nullptr` |
| `e.add_component<T>()` | 添加组件，返回命令缓冲中的临时实例 |
| `e.remove_component<T>()` | 移除组件 |
| `e.game_world()` | 取实体所在的世界 |

### 相关术语

参见[术语表](README.md)：

* [组件](component.md)——实体的构成单位；
* [预制体](prefab.md)——句柄持久的模板实体；
* [切片](slice.md)——系统每帧定位实体的方式；
* [世界](world.md)——实体所在的空间，以及结构变更的生效时机；
* [变换](transform.md)——用 uuid 组件表达父子关系的范例。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。实现主体位于 `src/jeecs_basic_arch_system.cpp`（`ArchType` / `arch_chunk` / `CommandBuffer`），句柄定义与行为注释见 `include/jeecs/ecs.hpp`，C++ 包装见 `include/jeecs/game.hpp`。

### 句柄三元组与版本号

`je_game_entity_t` 是 `{ _m_in_chunk, _m_id, _m_version }` 三元组：所属 chunk、chunk 内槽位、版本号。引擎内部的 `ArchType::entity` 与它布局完全一致（由一组 `static_assert` 保证），C ABI 边界处直接 `reinterpret_cast` 互转；C++ 的 `game_entity` 以组合方式持有它（`_m_raw`），并保持平凡、标准布局。

每个 chunk 为每个槽位保存一份元信息 `je_game_entity_meta_t { m_version, m_stat }`。句柄有效当且仅当**槽位当前版本等于句柄记录的版本**（`arch_chunk::is_entity_valid`）——它**不检查状态**，所以刚创建、尚未生效的实体也能正常取组件。版本号在两个时刻递增：

* 分配槽位时（`alloc_entity_id`）；
* 关闭槽位时（`command_close_entity`：状态置为 `UNAVAILABLE`、版本递增、槽位归还空闲栈）。

实体被搬动（换原型或碎片整理）的本质是“在新位置分配槽位、搬运组件、关闭旧槽位”，旧句柄因版本不符而失效。

### 实体状态三态

| 状态 | 含义 |
| --- | --- |
| `JE_ENTITY_STAT_UNAVAILABLE` | 槽位空闲（实体已销毁），或已分配但尚未生效（本帧刚创建） |
| `JE_ENTITY_STAT_READY` | 正常实体 |
| `JE_ENTITY_STAT_PREFAB` | 预制体（见[预制体](prefab.md)） |

C++ 切片、towoo 系统、`entity_selector` 的遍历，以及编辑器枚举世界实体用的 `je_debug_world_entities_get_all`，都**只认 `READY`**——新实体当帧不可见、预制体永远不可见，都是在这里实现的。

### 创建流程

`je_ecs_world_entity_create_with_components` 先把组件 ID 排序去重为 `TypesSet`，再调用 `World::create_entity_with_component(types, JE_ENTITY_STAT_READY)`：

1. `ArchManager::find_or_add_arch` 按 `TypesSet` 找到或新建原型；
2. `ArchType::instance_entity(nullptr)` 分配槽位并就地构造全部组件——**组件此刻已在 chunk 中**；
3. `CommandBuffer::init_new_entity` 登记目标状态，下一帧命令缓冲更新时由 `command_active_entity` 将其置为 `READY`。

槽位分配（`alloc_entity`）持原型的碎片整理共享锁：先以 CAS 递减原子空闲计数“占座”，再沿 chunk 链表从各 chunk 的无锁空闲槽位栈（带 ABA 计数的 Treiber 栈）中取出槽位；没有空位时新建 chunk 挂到链表头部。

### 结构变更的落地

组件增删与销毁写入实体级命令缓冲：`append_component` 立即 `malloc` 并构造一个新组件实例，以 `typed_component{类型, 实例}` 压入该实体的无锁链表（实例为空表示移除）；`remove_entity` 只设置 `m_entity_removed_flag`。

`CommandBuffer::update` 对各实体并行处理：

1. 链表按压入顺序逆序取出，按类型去重时**最后提交的操作胜出**，被覆盖的临时实例直接析构释放；
2. 句柄已失效：只释放临时实例；
3. 带销毁标记：析构临时实例与实体的全部组件，关闭槽位；
4. 否则先应用目标状态；若实体是预制体，拒绝全部增删（见[预制体](prefab.md)）；
5. 计算新的 `TypesSet`：与原型相同则原地替换（析构旧组件后，把新实例移动构造到原位）；不同则在新原型分配槽位，逐个移入旧组件或新实例，新槽位置为 `READY`，关闭旧槽位。

无论哪条路径，临时实例移入 chunk 后都会被 `free`——这就是 `add_component` 返回的指针**下一帧即悬垂**的原因。

### 碎片整理与悬垂句柄

实体级命令应用完毕后，`ArchManager::command_defragmentation` 对每个原型尝试整理：仅当空闲槽位超过两个 chunk 的容量、且不少于总槽位的一半时才执行。整理持独占锁，把较新 chunk 中 `READY` 的实体逐个搬进较早 chunk 的空位（移动构造、析构、关闭旧槽位）；非 `READY` 的占用槽位（预制体、尚未生效的实体）原地不动。被完全搬空的 chunk 会被**直接 `delete`**。

⚠️ 注意 `game_entity` 的注释称“世界仍存在时，失效的实体索引仍能进行各项操作，但均不生效”——这只在句柄指向的 chunk 仍存在时成立。chunk 被碎片整理释放后，旧句柄中的 `_m_in_chunk` 就是悬垂指针，`get_component`、`game_world()` 等都会读取已释放的内存。**跨帧持有句柄不只是“失效”，而可能是未定义行为**，这正是第一部分反复强调不要跨帧保存句柄的根本原因。

### 包装层

* C++ `game_entity` 的方法都是对 C ABI 的薄封装：组件操作以 `typing::id<T>()` 调用对应接口；`close()` 在 `_m_in_chunk` 为空时直接返回，否则经所属世界的 `remove_entity` 提交销毁；`==` 比较三元组的全部字段。
* Woolang `je::entity` 是 `gchandle`，持有一份堆上的 `je_game_entity_t` 拷贝，由 GC 释放；其 `==` 比较的是 `chunkinfo()` 字符串（`[chunk:槽位v版本]`）。
* 实体名的读写（`je_ecs_entity_name_get` / `je_ecs_entity_name_set`）直接返回 `Editor::Name` 中字符串的地址；缺少该组件时，`set` 先 `add_component`，名字写在命令缓冲的临时实例上。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 句柄、元信息、状态枚举与行为注释 | `include/jeecs/ecs.hpp` |
| `game_entity` / `game_world` C++ 包装 | `include/jeecs/game.hpp` |
| `ArchType` / `arch_chunk`、槽位分配、碎片整理 | `src/jeecs_basic_arch_system.cpp` |
| `CommandBuffer::update`（结构变更落地） | 同上 |
| Woolang 绑定 | `je/ecs.wo`、`je/towoo/towoo.wo`、`je/internal.wo`；原生侧 `src/jeecs_ecs_woolang_api.cpp` |
| 实体名 | `src/jeecs_core_systems_and_components.cpp` |
