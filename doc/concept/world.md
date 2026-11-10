# 世界（World）

本文档是[术语表](README.md)中「世界」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述世界的行为约定与用法；
* **第二部分·底层实现**——面向引擎维护者，描述上述行为在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 世界是什么

世界（World）是容纳*系统实例*与*组件实例*的空间，它是一切游戏内容的直接载体：实体存在于世界中，系统被注册到世界中，切片查询也以世界为单位进行。

每个世界都属于且仅属于一个*宇宙*（Universe）；一个宇宙可以同时容纳多个世界，这些世界的系统逻辑**并行**执行，互不等待。

### 世界与宇宙、系统的层级关系

```
宇宙（Universe，通常单例）
 ├── 世界 A
 │    ├── 系统实例（每类型一个，可随时增删）
 │    └── 实体
 ├── 世界 B
 │    ├── ...
 └── ...
```

* 宇宙负责驱动：帧循环、差异时间、以及对所有世界的并行调度；
* 世界负责内容：实体、系统；
* 系统负责逻辑：通过切片观察并修改本世界内的组件数据。

系统实例与世界一一绑定：把同一个系统类型分别注册进两个世界，会得到两个互相独立的实例。

### 世界的生命周期

#### 创建

在指定的宇宙中创建世界（Woolang：`world::create(universe)`；C++：`universe.create_world()`）。

**世界在创建之后默认是非激活状态**。请先完成初始化操作（注册系统、创建初始实体等），再调用启用（`set_enable(true)`）。编辑器同样遵循这个“先装系统、再启用”的流程。

#### 启用与停用

`set_enable(enable)` 控制世界是否参与运行：

* **启用**时，世界内所有系统的 on_enable 回调被依次调用，此后世界开始执行每帧的系统更新；
* **停用**时，所有系统的 on_disable 回调被依次调用，此后世界**完全暂停**——系统更新（包括渲染阶段）全部停止，但世界及其中的实体、系统都还活着，随时可以重新启用。

编辑器的调试运行（Play in Editor）正是利用了这一点：开始运行时先 `set_enable(false)` 暂停所有世界，趁暂停把“运行时系统”装进世界，再 `set_enable(true)` 恢复运行。

启用/停用请求经命令缓冲传递，在下一帧开始时生效（见[结构变更的生效时机](#结构变更的生效时机)）。

#### 销毁

`world::close(world)`（C++：`world.close()`）销毁世界。销毁是**异步**的：调用只是提交一个请求，真正销毁发生在下一帧开始时——先销毁所有系统，再销毁所有实体，最后把世界从宇宙中移除并释放。

向正在销毁中的世界创建实体或添加系统仍然会成功，但它们会随世界一并销毁——引擎不会为此报错。

### 结构变更的生效时机

对世界的结构性修改——添加/移除组件、创建/销毁实体、添加/移除系统、启用/停用、销毁世界本身——都**不是立即执行的**，而是先进入世界的命令缓冲，在下一帧开始时统一应用。这样设计是因为多个世界、多个系统正在并行运行，结构变更必须集中在一个安全的同步点上落地。

由此带来的行为细节：

* 同一帧内对系统增删的多次请求，**最后一次操作生效**；
* “添加系统”时：世界中不存在同类型系统则添加，已存在则替换实例；
* “移除系统”时：不存在同类型系统则无事发生。

结构变更之外的日常操作——读写组件成员数据——不受命令缓冲影响，可以在系统更新函数中直接进行。

### 一帧之中：世界何时被更新

宇宙把一帧划分为几个步骤，与世界相关的部分简化如下：

```
0. 世界更新    应用上一帧提交的结构变更（组件/实体/系统增删、启用停用、销毁）
1. 预先任务    引擎与宿主注册的前置工作
2. 常规任务    系统更新在这里执行（所有世界并行）
3. 提交任务    渲染呈现等收尾工作
```

也就是说，**在系统更新函数里提交的结构变更，要到下一帧的第 0 步才会生效**。

停用状态的世界会跳过上述所有投递给世界的工作，因此也不会执行系统更新。

#### 脚本系统的三个更新阶段

Woolang 脚本系统通过 `PRE_UPDATE!`、`UPDATE!`、`LATE_UPDATE!` 宏声明更新函数的执行时机，引擎按 PRE → UPDATE → LATE 的顺序依次调用；前一阶段的所有系统执行完毕后才会进入下一阶段，同一阶段内的系统则并行执行。

除这三个阶段外，引擎内置的 C++ 系统还占用变换结算、物理模拟、图形更新等内部阶段（完整顺序见[第二部分·系统更新的八个阶段](#系统更新的八个阶段)）。

### 实体句柄的有效期

**非预制体的实体句柄只保证在当前这一帧内有效**，不要跨帧持有；预制体（prefab）的句柄则是持久的。这是引擎碎片整理机制的结果：帧开始时实体可能被搬动位置，使旧句柄失效。

需要跨帧引用普通实体时，应改用其他方式（例如在当帧内取出所需的组件数据，或使用实体名字等间接手段）。

### 多世界的并行执行

* 宇宙对世界列表做并行遍历：**世界之间并行**；
* 每个世界内部，同一更新阶段的**系统之间并行**；
* 更新阶段之间串行（同步屏障）。

实体和系统实例都从属于单个世界，因此并行是安全的：两个世界之间没有共享的实体。**跨世界的协作不要通过直接访问另一个世界的实体完成**，应当通过宇宙层面注册的工作（call_once / for-worlds job）或其它消息机制。

典型用法：主游戏世界之外，用第二个世界承载 UI 逻辑、过场动画或后台模拟，让重度逻辑不阻塞主世界。

### API 速览

#### Woolang（`je::ecs`）

| 函数 | 说明 |
| --- | --- |
| `world::create(universe)` | 在宇宙中创建世界（默认非激活） |
| `world::close(world)` | 请求销毁世界（异步，下帧生效） |
| `world::set_enable(world, able)` | 启用/停用世界（经命令缓冲） |
| `world::add_system(world, systype)` | 注册系统实例（同类型重复添加为替换） |
| `world::remove_system(world, systype)` | 移除系统实例 |
| `world::get_system(world, systype)` | 获取已注册的系统实例 |
| `world::add_entity(world, components)` | 用给定组件集合创建实体 |
| `world::add_prefab(world, components)` | 创建预制体 |
| `world::instance(world, prefab)` | 依照预制体实例化新实体 |
| `world::get_universe(world)` | 取所属宇宙 |
| `world::create_selector(world, requirements)` | 创建持久化切片选择器（`entity_selector`） |
| `universe::get_all_worlds(universe)` | 列出宇宙中的所有世界 |

创建并启用一个世界的典型流程：

```woolang
import je;
import je::ecs;

let w = je::world::create(current_universe);
w->add_system(typeof:<MyGameSystem>::type::typeinfo);
// ... 按需 add_entity / add_prefab ...
w->set_enable(true);   // 一切就绪后再启用
```

#### C++（`jeecs::game_world` / `game_universe`，头文件 `jeecs/game.hpp`）

| 方法 | 说明 |
| --- | --- |
| `universe.create_world()` | 在宇宙中创建世界（默认非激活） |
| `world.close()` | 请求销毁世界（异步，下帧生效） |
| `world.set_enable(able)` | 启用/停用世界（经命令缓冲） |
| `world.add_system<SystemT>()` | 注册系统实例，返回类型化的 `SystemT*`（同类型重复添加为替换） |
| `world.get_system<SystemT>()` | 获取系统实例指针 |
| `world.remove_system<SystemT>()` | 移除系统实例 |
| `world.add_entity<CompTs...>()` | 以给定的组件类型集合创建实体 |
| `world.add_prefab<CompTs...>()` | 创建预制体 |
| `world.add_entity(prefab)` | 依照预制体实例化新实体 |
| `world.remove_entity(entity)` | 销毁实体 |
| `world.get_universe()` | 取所属宇宙 |

C++ 侧的创建流程：

```cpp
using namespace jeecs;

game_universe universe = game_universe::create_universe();
game_world     world   = universe.create_world();   // 默认非激活

world.add_system<MyGameSystem>();                   // 先装系统
world.add_entity<Transform::LocalPosition, Renderer::Shape>();  // 再建实体
world.set_enable(true);                             // 一切就绪后启用
```

### 相关术语

参见[术语表](README.md)：

* 宇宙——容纳所有世界的容器，负责帧循环与并行调度；
* 系统、系统更新——世界中的逻辑与其执行阶段；
* 实体、预制体——世界内容的构成单位；
* 切片——系统观察世界的窗口；
* 帧、工作——世界被驱动的方式。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。实现主体位于 `src/jeecs_basic_arch_system.cpp`，Woolang 侧绑定见 `je/ecs.wo` 与 `src/jeecs_woolang_towoo_script_basic.cpp`，C ABI 与各接口的行为注释见 `include/jeecs/ecs.hpp`。

### World 类与 C ABI

`World` 继承自空结构体 `je_game_world_t`——引擎以“空结构体作为公开基类、实现类继承”的方式暴露 C ABI 句柄，边界处可用 derived↔base 的 `static_cast`，无需 `reinterpret_cast`。

世界内部持有：

* `Universe*`——所属宇宙；
* `CommandBuffer`——命令缓冲（见[命令缓冲内部流程](#命令缓冲内部流程)）；
* `ArchManager`——原型/分块存储管理器（见[原型与分块存储](#原型与分块存储)）；
* `_m_arch_change_counter`——原型集合变更版本号，单调递增、从不消费；
* `_m_world_enabled` / `_m_destroying_flag`——激活与销毁标记（构造时默认**未激活**）；
* `m_systems`——`unordered_map<type_info*, je_system_t>`，已注册的系统实例；
* `m_system_slice_caches`——按系统实例分组的切片缓存（见[切片缓存与编译防火墙](#切片缓存与编译防火墙)）。

另有静态登记表 `_m_alive_worlds` 记录所有存活的世界实例；`World` 析构时断言 `_m_destroying_flag`——世界必须走完整的销毁流程，不允许直接 `delete`。

### 宇宙主循环中的一帧

宇宙的 `loop` 运行在专属线程上，一帧的完整顺序为：

```
0. update_universe_action_and_worlds()
   ├─ parallel_foreach 世界列表，执行 world->update()（见下节）
   ├─ 返回 false 的世界（已销毁）从列表移除并 delete
   └─ 处理宇宙动作队列：加入/移除世界、注册/注销各类工作
1. 预先任务（_m_shared_pre_jobs）
2. 常规任务（_m_shared_jobs）
   引擎在 Universe 构造时默认注册了“系统更新”工作（见下下节）
   [帧同步：_m_frame_deltatime > 0 时 sleep_until 帧起始时间，落后超限则追上当前时间]
3. 提交任务（_m_shared_after_jobs）
```

三类任务都通过 `parallel_foreach` 并行遍历：`FOR_WORLDS` 型工作对世界列表再开一层并行遍历、逐世界调用 `world->execute_world_job`；`CALL_ONCE` 型直接调用。`execute_world_job` 检查 `_m_world_enabled`——**停用的世界在这一步被整体跳过**，这就是停用世界不执行任何系统更新的实现位置。

渲染等呈现操作注册在提交任务（步骤 3）中。源码注释说明了时序约束：呈现必须在下一帧世界更新**之前**完成，避免图形资源因组件移除被释放后仍被采样，产生错误的渲染结果。

`World::update()` 的内部顺序：

1. `_m_command_buffer.update()`——应用结构变更并做碎片整理（见[命令缓冲内部流程](#命令缓冲内部流程)）；
2. 若 `_m_destroying_flag`：先移除所有系统实例，再 `close_all_entity` 销毁全部实体，然后**再执行一轮**命令缓冲更新（消费销毁过程中产生的命令）；
3. 若原型集合发生变化（`_m_arch_modified`）：`++_m_arch_change_counter`，并对所有切片缓存调用 `je_ecs_world_collection_update` 刷新；
4. 返回 `!_m_destroying_flag`（false 表示世界应被宇宙移除）。

### 系统更新的八个阶段

引擎默认注册的“系统更新”工作（`default_job_for_execute_sys_update_for_worlds_in_universe`，注册为常规 FOR_WORLDS 工作）在每个世界**内部**按以下顺序执行八个阶段。每个阶段是一次对该世界系统表的 `parallel_foreach`——阶段之间串行（天然同步屏障），阶段内系统并行：

| 顺序 | 更新器槽位 | 职责 |
| --- | --- | --- |
| 1 | `m_pre_update` | 逻辑前准备 |
| 2 | `m_state_update` | 状态更新 |
| 3 | `m_update` | 常规逻辑（最常用） |
| 4 | `m_transform_update` | 变换层级结算 |
| 5 | `m_physics_update` | 物理模拟 |
| 6 | `m_late_update` | 逻辑收尾 |
| 7 | `m_commit_update` | 数据提交 |
| 8 | `m_graphic_update` | 图形/渲染相关更新 |

⚠️ 注意 `je_typing_system_updater_register` 的**参数顺序**是 `... update, physics_update, transform_update, late_update ...`（physics 在 transform 之前），而**实际执行顺序**是 transform 在 physics 之前——以本表（即 job 内的调用顺序）为准，接线时不要按注册参数顺序类推。

系统还带两个非每帧回调：`on_enable` / `on_disable`，分别在系统加入已启用的世界、世界启用、以及对应的反向时机被调用。

### towoo 脚本系统的包装

每个 Woolang `system!` 声明都会在类型系统里注册一个 `ToWooBaseSystem` 类型的系统（`je_typing_register` + `je_typing_system_updater_register` 挂满全部更新器槽位），脚本函数按声明宏分组存入 `m_preworks` / `m_works` / `m_lateworks`，分别由 pre_update / update / late_update 槽位驱动——这就是脚本侧只见三个阶段的实现原因。

`_set_current_job_type` 强制注册顺序沿 PRE → UPDATE → LATE 单调递增，倒序声明会 panic。`work!` 声明的工作没有切片需求（`is_single_work`），不参与上述分组。

### 命令缓冲内部流程

命令缓冲（`CommandBuffer`）分两层：

* **实体级缓冲**（`_find_or_create_buffer_for_entity`）：组件增删（`typed_component`：类型 + 新组件实例指针；null 表示移除）；
* **世界级缓冲**（`_find_or_create_buffer_for_world`）：`m_destroy_world`、`m_update_enabled`（optional）、系统增删、切片缓存登记。

写入端（任意并行线程）持 `shared_lock` 追加命令；应用发生在 `World::update()` 的单线程阶段。`CommandBuffer::update()` 的应用顺序：

1. 应用所有实体命令（组件增删、新实体初始化、实体销毁）；
2. 清空实体缓冲，**执行碎片整理**（见[原型与分块存储](#原型与分块存储)）；
3. 应用世界命令：销毁标记 → `_ready_to_destroy()`；启用/停用 → `_set_enable_world()`（遍历系统调 on_enable/on_disable）；系统增删 → `append_system_instance` / `remove_system_instance`（见下）。

系统实例的落库规则：`append_system_instance` 在世界已启用时对新系统立即调用 `on_enable`，并为它建立切片缓存容器；同类型已存在时**替换**旧实例（旧实例按启用状态调 `on_disable` 后析构释放）。`remove_system_instance` 对不存在的类型无事发生。“一帧内最后一次操作生效”即命令按追加顺序应用的自然结果。

### 原型与分块存储

组件构成完全相同的实体被归入同一个 `ArchType`；原型持有 chunk 链表，实体在 chunk 内紧密排列，组件数据按类型分列存放——切片查询因此可以按原型批量圈定实体。

碎片整理（`command_defragmentation`）：持有 `_m_chunk_list_defragmentation_mx` 独占锁（防整理期间其他线程遍历 chunk 链表），把链表转存到临时队列，从尾到头取出空位回填存活实体，完成后把整理后的链表挂回去。**这正是实体句柄只能保证当帧有效的原因**：整理会搬动实体在 chunk 中的位置。

世界用两个标记表达“原型集合变了”：

* `_m_arch_modified`——测试并消费，仅供 `World::update()` 内部判断是否刷新切片缓存；
* `_m_arch_change_counter`——单调递增、从不消费，通过 `je_ecs_world_arch_change_version_get` 暴露给外部持久化查询方（如 `entity_selector`）：比较两次观测值即可判断是否需要调用 `je_ecs_world_collection_update` 重建缓存。

### 实体句柄的结构

`je_game_entity_t` 是 `{ chunk 指针, chunk 内槽位, 版本号 }` 三元组；实体元信息（`je_game_entity_meta_t`）带版本号与状态（`UNAVAILABLE` / `READY` / `PREFAB`）。碎片整理搬动实体后，旧句柄指向的槽位可能已被其他实体占用，版本号用于识别失效。预制体（`PREFAB`）不参与整理搬动，因此句柄持久。

### 切片缓存与编译防火墙

`m_system_slice_caches` 按 **(系统实例, 切片类型哈希)** 两级索引。源码注释解释了为何按系统实例分组：不同编译器/库对相同或不同的切片类型，其类型哈希可能相同也可能不同；按系统实例（即同一编译单元/模块）缓存，保证一个实例对应的缓存永远来自同一模块，规避编译防火墙两侧的哈希冲突。

`fetch_and_request_slice_cache_dependence` 只对缓存表做只读查找；未命中时新建 `HoldedRequirementCollection` 并经命令缓冲登记（因此**不允许在命令缓冲处理期间调用**该函数）。缓存的生命周期与系统实例一致，世界销毁时随系统一并释放。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| Universe / World / CommandBuffer / ArchManager / ArchType | `src/jeecs_basic_arch_system.cpp` |
| 系统更新 job、帧循环步骤 | 同上（`default_job_for_execute_sys_update_for_worlds_in_universe`、`update_universe_action_and_worlds`） |
| towoo 系统包装与 job 注册 | `src/jeecs_woolang_towoo_script_basic.cpp` |
| C ABI 与行为注释 | `include/jeecs/ecs.hpp`（逐项函数说明见 [doc/capi/ecs](../capi/ecs/README.md)） |
| Woolang 绑定 | `je/ecs.wo` |
| 编辑器暂停/恢复世界的实践参考 | `builtin/editor/Editor/DebugRun.wo` |
