# 系统（System）

本文档是[术语表](README.md)中「系统」与「系统更新」两个词条的详细说明——系统更新是系统被引擎驱动的方式，两者合并在本文中介绍。内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述系统的写法、更新阶段与生命周期约定；
* **第二部分·底层实现**——面向引擎维护者，描述系统类型的注册、更新器槽位与脚本系统的包装方式。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 系统是什么

系统（System）是纯粹的逻辑集合。系统本身不描述游戏中的任何事物，而是在每一帧通过*切片*（见 [slice.md](slice.md)）观察世界中满足条件的组件，读取或修改它们的成员数据，以此推动游戏状态向前演进。

* 系统以**类型**为单位注册进引擎，再以**实例**的形式加入世界：同一个世界中每个系统类型至多一个实例，同一类型加入两个世界会得到两个互相独立的实例（见 [world.md](world.md)）；
* 系统实例可以持有私有状态（计时器、配置等），但**游戏数据应当放在组件里**——系统只描述数据被怎样处理；
* 系统有两种写法：C++（继承 `jeecs::game_system`）与 Woolang 脚本（towoo 脚本系统）。

### 系统更新的八个阶段

*系统更新*（Update）是系统的逻辑函数，由引擎一帧一次地调用。每个世界的一次系统更新被划分为八个阶段，**阶段之间串行，阶段内的系统并行**；C++ 系统可以使用全部八个阶段，脚本系统只能使用其中三个：

| 顺序 | C++ 成员函数 | 脚本阶段宏 | 占用该阶段的内置系统 |
| --- | --- | --- | --- |
| 1 | `PreUpdate` | `PRE_UPDATE!` | — |
| 2 | `StateUpdate` | — | 动画、虚拟手柄输入、瓦片地图 |
| 3 | `Update` | `UPDATE!` | — |
| 4 | `TransformUpdate` | — | 变换结算（见 [transform.md](transform.md)）、UI 布局 |
| 5 | `PhysicsUpdate` | — | 2D 物理（见 [physics.md](physics.md)） |
| 6 | `LateUpdate` | `LATE_UPDATE!` | — |
| 7 | `CommitUpdate` | — | 变换矩阵提交、音频 |
| 8 | `GraphicUpdate` | — | 渲染管线、粒子（见 [render.md](render.md)） |

由这张表可以推出几条实用约定：

* 在 `UPDATE` 中修改局部变换后，同一阶段读到的世界变换仍是上一次结算的结果；需要**本帧结算后的世界坐标或物理结果**时，把逻辑放到 `LATE_UPDATE`；
* 同一阶段内各系统的执行顺序**没有保证**；若系统 B 依赖系统 A 本帧的结果，应把它们放到不同阶段；
* 系统更新只在**启用**状态的世界中执行（见[世界的启用与停用](world.md#启用与停用)）。

### 系统的生命周期

1. **构造**：调用 `add_system` 时立即构造实例（脚本系统此时执行 `create`）；但实例要到下一帧开始、并且所在世界处于启用状态时，才真正加入世界参与更新；
2. **on_enable**：实例加入已启用的世界，或所在世界由停用转为启用时调用；
3. **每帧更新**：按上表的阶段调用；
4. **on_disable**：所在世界被停用，或实例在启用状态下被移除、被同类型新实例替换、随世界销毁时调用；
5. **析构**：实例被移除、被替换或随世界销毁（脚本系统此时执行 `close`）。

on_enable / on_disable 在一个实例的一生中可能被调用多次（世界反复启停），适合做“开始/暂停”类的处理；一次性的初始化与清理应放在构造/析构（脚本的 `create` / `close`）中。

### 用 Woolang 编写系统

一个脚本系统就是一个 `.je4system.wo` 文件（编辑器的“新建 → ToWoo 系统”会生成模板），**文件名去掉后缀即系统的类型名**。文件中：

* `create(world) => Context` 与 `close(Context)` 必须提供，且以 `extern func` 声明：`create` 返回的 `Context` 是该系统实例的私有状态，会作为 `context` 传给每个工作函数；
* `on_enable(Context)` / `on_disable(Context)` 可选，同样以 `extern func` 声明；
* `PRE_UPDATE!` / `UPDATE!` / `LATE_UPDATE!` 切换其后声明所属的阶段，**必须按此顺序出现**（倒序会在注册时报错）；写在第一个阶段宏之前的声明属于 PRE_UPDATE；
* `system!` 声明一个*切片工作*：参数列表即视图（`T` 为必须包含，`T?` 为可有可无，对应参数类型为 `option<T>`），之后可接 `contains` / `except` / `anyof` 追加过滤条件；函数体对每个匹配的实体执行一次，体内可使用 `context` 与当前实体 `e`；
* `work!` 声明一个*全局工作*：没有切片，每次该阶段更新时执行一次，体内只有 `context`。

```woolang
import pkg::std;
import je;
import je::ecs;
import je::towoo;
import je::towoo::types;
import je::towoo::components;
import je::towoo::system;

using std;
using je;
using je::towoo;

using Context = struct{
    m_speed: real,
};

extern func create(_: je::world)=> Context
{
    return Context{ m_speed = 2., };
}
extern func close(_: Context)
{
}

UPDATE!
{
    // 对每个“有 LocalPosition、没有 LocalToParent”的实体执行一次
    system! move(localpos: Transform::LocalPosition, scale: Transform::LocalScale?)
    except Transform::LocalToParent;
    {
        let factor = scale->map_or(\s = s.scale->get[0];, 1.);
        let p = localpos.pos->get;
        localpos.pos->set(p + (context.m_speed * factor * je::deltatime(), 0., 0.) as vec3);
    }
    // 没有切片，每次 UPDATE 执行一次
    work! tick()
    {
        do context;
    }
}
```

同一阶段内的多个 `system!` / `work!` 按声明顺序依次执行。工作函数中可用 `je::deltatime()` 获取差异时间（见 [deltatime.md](deltatime.md)）；towoo 宏的完整语法见 [script.md](script.md)。

⚠️ 注意：工作函数（以及 on_enable / on_disable）中一旦发生 panic，该系统实例的脚本虚拟机会被关闭，**此后这个实例不再执行任何逻辑**；它仍留在世界中，直到被移除或被同类型的新实例替换。

### 用 C++ 编写系统

C++ 系统是继承 `jeecs::game_system` 的类型，构造函数接收 `game_world`。生命周期回调与各阶段更新**以固定的成员函数名声明**——`OnEnable`、`OnDisable` 与上表中的八个函数，均为无参、返回 `void`——只需写出用到的那些。它们不是虚函数，引擎在注册类型时按名字识别。

```cpp
#include "jeecs/game.hpp"
#include "jeecs/components.hpp"

namespace MyModule
{
    using namespace jeecs;
    using namespace jeecs::slice_requirement;

    struct MoverSystem : public game_system
    {
        float m_speed = 2.0f;   // 系统实例的私有状态

        MoverSystem(game_world world) : game_system(world) {}

        void OnEnable() {}      // 可选的生命周期回调
        void OnDisable() {}

        void Update()           // 只声明需要的阶段
        {
            const float dt = deltatime();
            for (auto&& [localpos, scale] : query<
                view typesof(Transform::LocalPosition&, Transform::LocalScale*),
                except typesof(Transform::LocalToParent)>())
            {
                const float factor = scale != nullptr ? scale->scale.x : 1.0f;
                localpos.pos.x += m_speed * factor * dt;
            }
        }
    };
}
```

`game_system` 提供 `get_world()`（所属世界）、`query<...>()` / `query_entity<...>()`（按切片遍历组件，见 [slice.md](slice.md)）以及 `deltatime()` / `smooth_deltatime()` / `timescale()`（宇宙的时间信息，见 [deltatime.md](deltatime.md)）。C++ 系统类型需要在模块入口中以 `typing::register_type<MyModule::MoverSystem>(guard, "MyModule::MoverSystem")` 注册（原生模块的注册代码由编辑器生成，见 [module.md](module.md)）。

⚠️ 注意：成员函数名或签名写错（例如 `update()`、带参数或有返回值）不会产生编译错误，只会被引擎当作“没有实现该阶段”而静默忽略。

### 把系统加入世界

| 操作 | Woolang（`je::ecs`） | C++（`jeecs::game_world`） |
| --- | --- | --- |
| 添加（同类型重复添加为替换） | `w->add_system(systype)` | `world.add_system<SystemT>()` |
| 查询实例 | `w->get_system(systype)` | `world.get_system<SystemT>()` |
| 移除 | `w->remove_system(systype)` | `world.remove_system<SystemT>()` |

Woolang 侧的系统类型信息按名字取得：`je::typeinfo::load("Mover")->unwrap`（内置系统的名字形如 `Translation::TranslationUpdatingSystem`）。几个时序细节：

* `add_system` 返回时实例已经构造完毕（C++ 版直接返回实例指针），但 `get_system` 要等实例在下一帧开始时落入世界后才能查到；
* 对**停用**的世界添加/移除系统，请求会一直挂起，直到世界被启用时才生效——这正是“先装系统、再启用世界”流程成立的原因（见 [world.md](world.md#世界的生命周期)）。

### 并行与数据安全

* 同一阶段的系统并行执行，引擎**不会**为它们访问的组件加锁或排序：两个同阶段系统读写同一种组件数据，会产生数据竞争；
* 并行只在 Release 构建中启用（Debug 构建退化为串行执行），因此这类问题可能只在 Release 下暴露；
* 每个脚本系统实例拥有独立的虚拟机，脚本系统之间并行执行不会争用虚拟机（组件数据的竞争仍按上一条处理）；同一脚本系统内的工作则串行执行；
* 跨世界的协作不要在系统中直接访问另一个世界的实体（见[多世界的并行执行](world.md#多世界的并行执行)）。

### 相关术语

参见[术语表](README.md)：

* 世界——系统实例的容器（[world.md](world.md)）；
* 切片——系统观察世界的窗口（[slice.md](slice.md)）；
* 组件——系统读写的数据（[component.md](component.md)）；
* 帧、工作——系统更新本身就是宇宙中的一项工作（[frame.md](frame.md)）；
* 差异时间——系统中最常用的时间量（[deltatime.md](deltatime.md)）；
* 脚本——towoo 宏的全貌（[script.md](script.md)）；
* 变换——占用变换阶段的内置结算系统（[transform.md](transform.md)）。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。系统类型的注册模板位于 `include/jeecs/typing.hpp`（`default_functions`、`register_type`），C++ 基类见 `include/jeecs/game.hpp`，系统实例的落库与系统更新工作位于 `src/jeecs_basic_arch_system.cpp`，脚本系统包装位于 `src/jeecs_woolang_towoo_script_basic.cpp` 与 `je/towoo/system.wo`。

### 系统类型与更新器槽位

系统在类型系统中的类别是 `JE_SYSTEM`。`je_type_info_t::m_system_updaters` 指向 `je_typeinfo_system_updater_t`：两个回调（`m_on_enable` / `m_on_disable`）加八个更新器槽位，由 `je_typing_system_updater_register` 填写。

C++ 侧的 `register_type<T>` 以 `std::is_base_of_v<game_system, T>` 判定类别；对系统类型，用 `default_functions<T>` 的十个静态函数**填满全部槽位**。每个函数体内用 concept（`has_OnEnable` … `has_GraphicUpdate`，要求 `{ t.Xxx() } -> std::same_as<void>`）做 `if constexpr` 检测：声明了对应成员函数就转发调用，否则函数体为空。由此带来两个结果：

* 槽位永远非空，系统更新工作对每个系统的每个阶段都会无条件调用一次，未实现的阶段只是一次空调用；
* 名字或签名不符的成员函数检测不到，被静默当作未实现——这就是第一部分那条警告的来源。

系统实例构造时，`default_functions<T>::constructor` 收到的参数是所属世界的 `je_game_world_t*`；`game_world` 可由它隐式构造，所以系统类型提供 `T(game_world)` 即满足 `has_game_system_constructor_function`。

⚠️ 注意 `je_typing_system_updater_register` 的参数顺序是 `... update, physics_update, transform_update, late_update ...`，与实际执行顺序（transform 在 physics 之前）不一致；执行顺序以[世界文档的八阶段表](world.md#系统更新的八个阶段)为准。

### 系统实例的创建与落库

`World::request_to_append_system` 先校验类别为 `JE_SYSTEM`（否则返回 nullptr，Woolang 的 `add_system` 因此返回 false），随后 `malloc` + `typing::construct(type, sys, world)`——**在调用线程立即构造实例**，再把（类型, 实例）追加进世界命令缓冲。`je_ecs_world_system_instance_get` 直接查 `m_systems`，所以落库之前查不到。

落库发生在下一帧 `World::update()` → `CommandBuffer::update()` 的世界命令阶段：先应用启用/停用，然后**仅当世界已启用或正在销毁时**才处理系统增删与切片缓存登记；否则整个世界命令缓冲原样保留到之后的帧。处理时同一类型的多个请求只保留最后一个，被覆盖的“添加”请求直接析构其实例（不经过 on_enable / on_disable）。`append_system_instance` 在世界已启用时立即调用 on_enable 并建立该实例的切片缓存容器；`_destroy_system_instance` 在世界已启用时先调 on_disable，再析构并 `free`。

系统更新工作（`default_job_for_execute_sys_update_for_worlds_in_universe`）对 `m_systems` 依次做八次 `parallel_foreach`，见[世界文档](world.md#系统更新的八个阶段)。`jeecs::parallel_foreach` 仅在 `JE_PARALLEL_FOREACH_ENABLED`（有 `<execution>`、启用了异常、非 WebGL、且定义了 `NDEBUG`）时使用 `std::execution::par_unseq`，否则为串行的 `std::for_each`。

### towoo 脚本系统的包装

**一个脚本文件对应一个系统类型**。`je_towoo_system_register(name, path)` 编译脚本，加载 `_init_towoo_system`（定义在 `je/towoo/system.wo`）、`create`、`close`（缺任一即注册失败）与可选的 `on_enable` / `on_disable`；随后先注销同名的旧类型（因此重复注册即为更新），再以 `name` 注册一个尺寸为 `sizeof(ToWooBaseSystem)` 的 `JE_SYSTEM` 类型并挂满更新器槽位。类型级信息保存在 `towoo_system_info` 中：编译产物、上述函数，以及按阶段分组的 `m_preworks` / `m_works` / `m_lateworks`。

工作的收集发生在脚本顶层代码执行时（注册期间执行一次）：

* 阶段宏只是展开为一句 `_set_current_job_type(...)`，后面的花括号是普通代码块。它修改脚本全局的“当前阶段”（初值 PRE_UPDATE，只允许单调不减，倒序时 `std::panic`）；
* `system!` / `work!` 展开为 `register_job_function` / `register_work_function` 调用加一个函数定义，记录声明时的当前阶段与需求列表（参数、`contains`、`except`、`anyof` 依次分配需求分组号）；
* `_init_towoo_system` 把这些记录逐条交给原生侧 `wojeapi_towoo_register_system_job`，按阶段放入三个分组。

`towoo_step_work::m_work_sequence` 是每个系统类型内自增的工作序号：非零时既标识这是切片工作，也作为该实例在世界切片缓存中的键（`je_ecs_world_slice_dependence_query` 的哈希参数）；`work!` 的序号为 0。

实例级的 `ToWooBaseSystem` 继承 `game_system`，只实现 `OnEnable` / `OnDisable` / `PreUpdate` / `Update` / `LateUpdate`——**这就是脚本只能使用三个阶段的原因**。每个实例构造时创建独立的虚拟机并调用 `create(world)` 得到 context，析构时调用 `close`。`update_step_work` 按声明顺序串行执行该阶段的工作：`work!` 直接调用；切片工作遍历缓存的原型与 chunk，跳过非 READY 的实体，为每个实体构造组件包装与实体句柄后调用。任一调用失败即 `_drop()` 关闭虚拟机，此后 `m_job_vm == nullptr`，所有更新与回调都直接返回（`close` 也不再被调用）。

`update_step_work` 期间，`thread_local` 的 `current_script_game_system_instance` 指向当前实例，`je::deltatime()` 等接口依赖它取时间；在工作函数之外（包括 on_enable / on_disable）调用这些接口会 panic。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| C++ 系统基类 `game_system`（query、时间访问） | `include/jeecs/game.hpp` |
| 更新器槽位结构、concept 检测、`register_type` | `include/jeecs/typing.hpp` |
| 系统实例落库、系统更新工作 | `src/jeecs_basic_arch_system.cpp`（`request_to_append_system`、`CommandBuffer::update`、`default_job_for_execute_sys_update_for_worlds_in_universe`） |
| 脚本系统包装与工作注册 | `src/jeecs_woolang_towoo_script_basic.cpp`（`ToWooBaseSystem`、`je_towoo_system_register`、`wojeapi_towoo_register_system_job`） |
| towoo 系统宏 | `je/towoo/system.wo` |
| 内置系统类型注册 | `src/jeecs_core_systems_and_components.cpp` |
| 系统模板（脚本 / 原生） | `builtin/editor/Editor/Template/towoo_system.je4system.wo.template`、`native_module_system.{hpp,cpp}.template` |
| C ABI 逐项说明 | [doc/capi](../capi/README.md)（typing、ecs 分册） |
