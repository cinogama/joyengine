# 世界（World）

本文档是[术语表](README.md)中「世界」一词的详细说明。

## 世界是什么

世界（World）是容纳*系统实例*与*组件实例*的空间，它是一切游戏内容的直接载体：实体存在于世界中，系统被注册到世界中，切片查询也以世界为单位进行。

每个世界都属于且仅属于一个*宇宙*（Universe）；一个宇宙可以同时容纳多个世界。宇宙在一帧内会对所有世界发起工作调用，这些调用是**并行**执行的——多个世界的系统逻辑会同时运行，互不等待。

一个世界内部持有三样东西：

* **实体**——以*原型*（ArchType）和*分块*（Chunk）的形式存储（见下文[实体的存储](#实体的存储原型与分块)）；
* **系统实例**——每个被注册进世界的系统类型对应一个实例；
* **切片缓存**——为每个系统实例缓存的切片查询结果，当世界的原型集合发生变化时自动刷新。

## 世界与宇宙、系统的层级关系

```
宇宙（Universe，通常单例）
 ├── 世界 A
 │    ├── 系统实例（每类型一个，可随时增删）
 │    └── 实体（按原型分块存储）
 ├── 世界 B
 │    ├── ...
 │    └── ...
 └── ...
```

* 宇宙负责驱动：帧循环、差异时间、以及对所有世界的并行调度；
* 世界负责内容：实体、系统、切片；
* 系统负责逻辑：通过切片观察并修改本世界内的组件数据。

系统实例与世界一一绑定：把同一个系统类型分别注册进两个世界，会得到两个互相独立的实例。

## 世界的生命周期

### 创建

在指定的宇宙中创建世界（Woolang：`world::create(universe)`；C API：`je_ecs_world_create`）。

**世界在创建之后默认是非激活状态**。请先完成初始化操作（注册系统、创建初始实体等），再调用启用（`set_enable(true)`）。编辑器同样遵循这个“先装系统、再启用”的流程。

### 启用与停用

`set_enable(enable)` 控制世界是否参与运行，生效时机在下一轮世界更新（经过命令缓冲，见下文）：

* **启用**时，世界内所有系统实例的 on_enable 会被依次调用，此后世界开始执行被投递给自己的各项工作（包括每帧的系统更新）；
* **停用**时，所有系统的 on_disable 被依次调用，此后世界**不再执行任何投递给世界的工作**——系统更新（包括渲染阶段）全部暂停，但世界及其中的实体、系统都还活着，随时可以重新启用。

编辑器的调试运行（Play in Editor）正是利用了这一点：开始运行时先 `set_enable(false)` 暂停所有世界，趁暂停把“运行时系统”装进世界，再 `set_enable(true)` 恢复运行。

### 销毁

`world::close(world)` / `je_ecs_world_destroy` 销毁世界。销毁是**异步**的：调用只是向命令缓冲提交一个请求，真正销毁发生在下一次世界更新时，顺序为：

1. 世界被标记为即将销毁；
2. 销毁所有系统实例；
3. 销毁所有实体；
4. 执行最后一轮命令缓冲更新；
5. 被宇宙从世界列表中移除并释放。

向正在销毁中的世界创建实体或添加系统仍然会成功，但它们会在下一次世界更新时随世界一并销毁——引擎不会为此报错。

## 命令缓冲：结构变更为何延迟生效

对世界的结构性修改——添加/移除组件、创建/销毁实体、添加/移除系统、启用/停用、销毁世界本身——都不是立即执行的，而是先写入世界的**命令缓冲**（Command Buffer），在下一次世界更新时统一应用。

这样设计是因为多个世界、多个系统正在并行运行，结构变更必须集中在一个安全的同步点上落地。由此带来的行为细节：

* 同一帧内对系统增删的多次请求，**最后一次操作生效**；
* “添加系统”时：世界中不存在同类型系统则添加，已存在则替换实例；
* “移除系统”时：不存在同类型系统则无事发生。

结构变更之外的日常操作——读写组件成员数据——不受命令缓冲影响，可以在系统更新函数中直接进行。

## 一帧之中：世界的更新顺序

宇宙的主循环把一帧划分为四个步骤，世界的参与方式如下：

```
0. 世界更新        应用命令缓冲：组件/实体/系统增删、启用停用、世界销毁、
                  碎片整理、切片缓存刷新；移除并释放已销毁的世界
1. 预先任务        （pre_for_worlds / pre_call_once 工作，并行）
2. 常规任务        （for_worlds / call_once 工作，并行）
                  └─ 引擎默认注册的“系统更新”工作：对每个世界并行执行，
                     世界内按下述八个阶段依次推进
3. 提交任务        （after_for_worlds / after_call_once 工作，并行；
                  渲染呈现等操作在这里完成，位于世界更新之前、避免渲染
                  到已被销毁的图形资源）
```

停用状态的世界会跳过投递给自己的所有工作（步骤 1/2/3 中的 for-worlds 部分），因此也不会执行系统更新。

### 系统更新的八个阶段

引擎内置的系统更新工作在**每个世界内部**按以下顺序执行八个阶段，阶段之间是同步屏障，前一阶段全部完成后才进入下一阶段；**每个阶段内部，世界里的各个系统并行执行**：

| 顺序 | 阶段 | 职责 |
| --- | --- | --- |
| 1 | pre_update | 逻辑前准备 |
| 2 | state_update | 状态更新 |
| 3 | update | 常规逻辑（最常用） |
| 4 | transform_update | 变换层级结算 |
| 5 | physics_update | 物理模拟 |
| 6 | late_update | 逻辑收尾 |
| 7 | commit_update | 数据提交 |
| 8 | graphic_update | 图形/渲染相关更新 |

Woolang 脚本系统只暴露其中三个入口：`PRE_UPDATE!`、`UPDATE!`、`LATE_UPDATE!` 宏分别把系统函数挂到 pre_update、update、late_update 阶段；其余阶段供引擎内置的 C++ 系统使用（例如变换系统占 transform_update、物理系统占 physics_update、渲染系统占 graphic_update）。

## 实体的存储：原型与分块

世界中组件构成完全相同的实体，会被集中存放在同一个**原型**（ArchType）下；每个原型把内存切分为若干**分块**（Chunk），实体在分块中紧密排列。这种布局让切片查询可以按原型批量定位实体，而不必逐个检查。

每当世界中诞生一种新的组件组合（新原型）时，世界的**原型变更版本号**（单调递增，`je_ecs_world_arch_change_version_get`）就会递增。持有持久化切片查询的一方（例如 Woolang 的 `entity_selector`）只需比较两次观测到的版本号，即可判断缓存是否需要刷新——未变化时复用缓存，变化时调用 `je_ecs_world_collection_update` 重建。

## 实体句柄的有效期

实体句柄（`je_game_entity_t`）本质上是“分块指针 + 槽位 + 版本号”。由于世界更新时会进行**碎片整理**（把分块中的空洞填平、实体被搬动），**非预制体的实体句柄只保证在当前这一帧内有效**，不要跨帧持有；预制体（`JE_ENTITY_STAT_PREFAB`）的句柄则是持久的。

需要跨帧引用普通实体时，应改用其他方式（例如在当帧内取出所需的组件数据，或使用实体名字等间接手段）。

## 多世界的并行执行

* 宇宙对世界列表做并行遍历：**世界之间并行**；
* 每个世界内部，同一阶段的**系统之间并行**；
* 阶段之间串行（同步屏障）。

实体和系统实例都从属于单个世界，因此并行是安全的：两个世界之间没有共享的实体，无需加锁。跨世界的协作应当通过宇宙层面注册的工作（call_once / for-worlds job）或其它消息机制完成，而不是直接去触碰另一个世界的实体。

典型用法示例：主游戏世界之外，用第二个世界承载 UI 逻辑、过场动画或后台模拟，让重度逻辑不阻塞主世界。

## API 速览

### Woolang（`je::ecs`）

| 函数 | 说明 |
| --- | --- |
| `world::create(universe)` | 在宇宙中创建世界（默认非激活） |
| `world::close(world)` | 请求销毁世界（异步，下轮生效） |
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

### C API（`je_ecs_world_*`）

对应关系：`je_ecs_world_create` / `je_ecs_world_destroy` / `je_ecs_world_enable_set` / `je_ecs_world_system_instance_add|get|remove` / `je_ecs_world_entity_create_with_components` / `je_ecs_world_entity_create_with_prefab` / `je_ecs_world_prefab_create_with_components` / `je_ecs_world_entity_destroy` / `je_ecs_world_entity_component_add|get|remove` / `je_ecs_world_of_entity` / `je_ecs_world_in_universe`，以及切片查询相关的 `je_ecs_requirements_collect` / `je_ecs_world_collection_update` / `je_ecs_world_arch_change_version_get` / `je_ecs_world_slice_dependence_query`。逐项说明见 [doc/capi/ecs](../capi/ecs/README.md)。

## 相关术语

参见[术语表](README.md)：

* 宇宙——容纳所有世界的容器，负责帧循环与并行调度；
* 系统、系统更新——世界中的逻辑与其执行阶段；
* 实体、预制体——世界内容的构成单位；
* 切片——系统观察世界的窗口；
* 帧、工作——世界被驱动的方式。
