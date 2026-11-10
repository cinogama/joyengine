# 脚本（Script）

本文档是[术语表](README.md)中「脚本」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述脚本的种类、towoo 宏的写法与组织约定；
* **第二部分·底层实现**——面向引擎维护者，描述脚本如何被编译、绑定到引擎，并包装为原生的组件与系统。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 脚本是什么

脚本（Script）是用 *Woolang* 编写的 `.wo` 源文件。JoyEngine 的游戏逻辑以脚本为主：[组件](component.md)与[系统](system.md)主要在脚本中声明；[入口](entry.md)、[着色器](shader.md)（`.shader` 文件）、物理配置（`.je4physics.wo`）乃至[编辑器](editor.md)本身，也都是 Woolang 脚本。

脚本通过引擎随附的 `je` 系列模块访问引擎能力，其中专门服务于"在脚本里写组件与系统"的一层称为 *towoo*，它提供了 `component!`、`system!`、`work!` 等宏。

### 引擎脚本库一览

引擎脚本库位于引擎目录的 `je/` 下，以 `import je::xxx;` 导入：

| 模块 | 内容 |
| --- | --- |
| `je` | 日志、时间与差异时间、平台、文件读取、原生模块装载、towoo 类型注册 |
| `je::typeinfo` / `je::ecs` | 类型信息查询；宇宙、世界、实体、组件与实体选择器 |
| `je::graphic` / `je::shader` | 纹理、网格、着色器等图形资源；编写 `.shader` 着色器所用的声明式接口 |
| `je::audio` / `je::input` / `je::animation` / `je::tilemap` | 音频、输入、动画播放、瓦片地图 |
| `je::gui` / `je::internal` / `je::cache` | 即时模式界面（[GUI](gui.md)）；编辑器向的内部接口；缓存生成用的二进制写入器 |
| `je::physics2d::config` | 物理场景配置脚本的声明宏 |
| `je::towoo` 及其子模块 | 组件成员访问、数学类型运算、`component!` / `system!` / `work!` 宏 |

以 `import pkg::xxx;` 导入的则是第三方 Woolang [包](package.md)，例如标准库 `pkg::std`。

### 编写 ToWoo 组件

组件脚本以 `.je4component.wo` 为后缀，**文件名（去掉后缀）即组件名**。用 `component!` 宏逐个声明成员：

```woolang
// Player.je4component.wo
import pkg::std;

import je::towoo::component;
import je::towoo::types;

component!
{
    speed: float,
    hp: int32,
}
```

成员类型必须是引擎登记过脚本解析器的类型，例如 `bool`、`int8`～`int64`、`uint8`～`uint64`、`float`、`real`、`string`、`vec2`/`vec3`/`vec4`、`ivec2`、`quat`、`color`（完整列表见自动生成的 `je::towoo::types`）。注意这里**没有裸 `int`**，整数成员请使用带宽度的名字。在类型前加 `woolang` 关键字并给出初值（`name: woolang T = 初值`），可以声明直接保存 Woolang 值的成员。ToWoo 组件**不能与原生（C++）组件同名**，否则注册失败。

### 编写 ToWoo 系统

系统脚本以 `.je4system.wo` 为后缀，文件名即系统名。脚本由三部分构成：以 `extern func` 声明的生命周期函数（必需的 `create`、`close`，可选的 `on_enable`、`on_disable`）；`PRE_UPDATE!`、`UPDATE!`、`LATE_UPDATE!` 三个阶段宏及其后的代码块；以及阶段块内的 `system!`（按切片逐实体调用）与 `work!`（每帧调用一次）声明。

```woolang
// PlayerSystem.je4system.wo
import pkg::std;

import je;
import je::ecs;
import je::input;
import je::towoo;
import je::towoo::types;
import je::towoo::components;
import je::towoo::system;

using je;
using je::towoo;

using Context = struct{
    m_frame_count: mut int,
};

extern func create(_: je::world)=> Context
{
    return Context{ m_frame_count = mut 0 };
}
extern func close(_: Context)
{
}

PRE_UPDATE!
{
    work! count_frame()
    {
        context.m_frame_count += 1;
    }
}
UPDATE!
{
    // 带 LocalPosition 与 Player、且不带 LocalToParent 的实体，每帧逐个调用一次
    system! move_player(
        localpos: Transform::LocalPosition,
        player: Player,
        localscale: Transform::LocalScale?,
    )
    except Transform::LocalToParent;
    {
        if (!je::input::keydown(keycode::D))
            return;

        let mut step = player.speed->get * je::deltatime();
        match (localscale)
        {
            value(s)? step = step * s.scale->get[0];
            none?;
        }
        localpos.pos->set(localpos.pos->get + (step, 0., 0.) as vec3);
    }
}
```

* `create(world)` 在系统实例创建时调用，返回值是该实例的*上下文*（context），之后作为 `context` 传给 `close`、`on_enable`/`on_disable` 以及每个 `system!` / `work!` 的函数体。**实例自己的状态请放在上下文里**，不要放在脚本全局变量中。
* `system! 名字(参数列表) 过滤条件 { 函数体 }`：参数 `名字: 组件类型` 表示"必须包含"，类型后加 `?` 表示"可有可无"（参数变为 `option<组件类型>`）。参数列表之后可以追加 `contains A, B;`、`except A, B;`、`anyof A, B;` 三种不作为参数的过滤条件，与[切片](slice.md)的四种过滤一一对应。函数体内还可以使用隐含参数 `e`（当前实体）。
* `work! 名字() { 函数体 }` 不遍历实体，每帧只调用一次。
* 组件参数的成员是"成员句柄"：`->get` 读取、`->set(值)` 写入，直接作用于世界中的组件数据。
* 阶段宏必须按 PRE → UPDATE → LATE 的顺序出现（同一阶段可以出现多次），倒序会使注册失败；写在任何阶段宏之前的声明归入 PRE_UPDATE。三个阶段的执行时机见[世界](world.md#脚本系统的三个更新阶段)与[系统](system.md)。
* `je::deltatime()` / `je::smooth_deltatime()` 只能在脚本系统的更新函数中调用（见[差异时间](deltatime.md)）。

### 脚本的组织约定

* **组件 → 刷新声明 → 系统**：系统脚本通过 `je::towoo::components` 看到全部组件，因此必须先注册组件、刷新 towoo 声明，再编译系统。编辑器打开项目时自动按此顺序处理；手写入口脚本时需要自己遵守（见[入口](entry.md)）。
* 编辑器每隔数秒检查已注册脚本的修改时间，冻结世界后重新编译**发生变化的**脚本（同样是先组件、再刷新声明、后系统）。组件结构改变后，引用它的系统若没有一同修改，并不会被自动重编译——此时请使用菜单「编辑 → 重新编译 ToWoo」强制全部重编译。
* 先于项目类型注册执行的脚本（典型的是入口脚本）**不要 import `je::towoo` 或 `je::towoo::components`**——后者在导入阶段就会按名字查找全部组件类型，此时项目类型尚未注册（原因见第二部分）。
* 项目中还有两份类型扩展脚本：`resource/editor/types.wo`（各类型的序列化）与 `resource/editor/editor.wo`（各类型在检视面板中的编辑控件），新建项目时从引擎内置副本拷贝而来，可按需修改。

### API 速览

#### Woolang（`je`、`je::towoo`）

| 函数 | 说明 |
| --- | --- |
| `je::towoo::update_component(name, path)` | 编译并注册（或更新）ToWoo 组件，返回 `option<typeinfo>` |
| `je::towoo::update_api()` | 按当前类型注册表重新生成 `je::towoo::types` / `je::towoo::components` |
| `je::towoo::update_system(name, path)` | 编译并注册（或替换同名的）ToWoo 系统，返回 `option<typeinfo>` |
| `e->towoo::add_component:<T>()` / `get_component:<T>()` / `remove_component:<T>()` | 强类型地增、查、删实体上的组件（增删属于结构变更，下一帧生效，见[世界](world.md#结构变更的生效时机)） |
| `T::type::typeinfo` / `je::typeinfo::load(name)` | 取组件的类型信息 / 按名字取任意类型（含系统）的类型信息 |

#### C++（`jeecs::typing`，头文件 `jeecs/typing.hpp`、`jeecs/entry.hpp`）

C++ 组件只要登记了成员，就会以同样的形式出现在 `je::towoo::components` 中：

```cpp
namespace Game
{
    struct Health
    {
        JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Health);
        JECS_DEFAULT_CONSTRUCTOR(Health);

        float hp = 100.f;

        static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
        {
            jeecs::typing::register_member(guard, &Health::hp, "hp");
        }
    };
}

// 注册类型后刷新脚本声明，此后脚本中可以写 health.hp->get
jeecs::typing::register_type<Game::Health>(guard, "Game::Health");
jeecs::entry::module_entry(guard);
```

`typing::register_type` 首次注册时会调用 `T::JERefRegsiter`；只有带脚本解析器的成员类型才会暴露给脚本；`entry::module_entry` 登记内置类型并刷新 towoo 声明，是[模块](module.md)入口的固定步骤。

### 相关术语

参见[术语表](README.md)：

* [组件](component.md)、[系统](system.md)——脚本最主要的产物；
* [世界](world.md)、[切片](slice.md)——系统函数的执行环境与实体筛选方式；
* [入口](entry.md)——引擎启动时执行的首个脚本；
* [编辑器](editor.md)——负责扫描、注册与热更新项目脚本；
* [包](package.md)、[模块](module.md)——脚本的第三方依赖与原生扩展。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。towoo 的实现主体位于 `src/jeecs_woolang_towoo_script_basic.cpp`；引擎 API 的 Woolang 绑定按模块分布在 `src/jeecs_*_woolang_api.cpp`，脚本侧声明在 `je/` 目录；绑定表由 `script/prebuild/prebuild.wo` 在 CMake 配置阶段生成。

### towoo 的含义

源码注释写道："toWoo JoyEngine generated wrap code for woolang"——towoo 即 "to Woo(lang)"，指引擎为 Woolang 生成的包装层。它包括两部分：根据类型注册表生成的脚本声明（`je/towoo/types.wo`、`je/towoo/components.wo`），以及把脚本组件、脚本系统包装成引擎类型的 `ToWooBaseComponent` / `ToWooBaseSystem`。

### extern 绑定与 libjoyecs

脚本侧以 `extern("libjoyecs", "wojeapi_xxx")` 声明函数，C++ 侧在对应模块的绑定文件中实现同名的 `WOORT_API woort_api wojeapi_xxx(void)`（`je_gui_*` 同理），按下标取参数、以 `woort_ret_*` 返回：

```cpp
WOORT_API woort_api wojeapi_load_module(void)
{
    auto* const dylib_handle = je_module_load(woort_string(0), woort_string(1));
    if (dylib_handle == nullptr)
        return woort_ret_option_none();
    return woort_ret_option_pointer(dylib_handle);
}
```

`libjoyecs` 并不是一个真实的动态库。prebuild 用正则扫描 `je/` 下全部脚本中的 `extern("库名", "函数名")`，生成 `src/jeecs_extern_library_woo_api.hpp`：为每个函数生成声明，并构造一张函数表，以 `woort_dylib_fake("libjoyecs", ...)` 注册为伪造库。引擎启动时（`engine_woolang_api` 子系统）完成注册；Woolang 解析 extern 时按库名在 woort 的库注册表中命中它，从而绑定到引擎内的函数。

⚠️ 注意：在 `je/` 中新增 extern 声明后，必须在 C++ 中提供同名实现（否则链接失败），并重新执行 CMake 配置以重新生成函数表。另外，扫描正则只认 `extern("库", "函数")` 的字面形式——像 `towoo.wo` 中按参数类型编译期选择函数名的条件式 extern，必须在旁边以注释 `/* extern("libjoyecs", "wojeapi_...") */` 列出全部候选函数名，否则它们不会进入函数表。

### types.wo 与 components.wo 的生成

`je_towoo_api_update()` 遍历类型注册表：

* `types.wo`：每个带脚本解析器的类型输出其 Woolang 声明，外加 `namespace 类型名 { using type = void { public let typeinfo = je::typeinfo::load("类型名")->unwrap; } }`；
* `components.wo`：每个组件按类型名拆出命名空间，生成 `using 组件名 = struct{ public __addr: handle, public 成员: je::towoo::member<值类型, 成员类型::type>, ... }`，同样附带 `type::typeinfo`。

两个文件以 `woort_vfs_create` 写入 Woolang 的**虚拟文件系统**；Debug 构建在桌面平台上还会把它们写回源码树的 `je/towoo/` 供 IDE 补全（因此它们被 `.gitignore` 忽略）。触发时机有两个：`jeecs::entry::module_entry` 的末尾（引擎启动、模块入口），以及脚本调用 `je::towoo::update_api()`。

prebuild 把 `je/` 拷贝到构建目录时**刻意排除**这两个文件：Woolang 的 import 解析依次尝试导入方目录、工作目录、可执行文件目录，最后才是虚拟文件系统，构建目录里若残留旧副本就会遮蔽最新生成的版本。

⚠️ 注意：`components.wo` 顶层的 `typeinfo = load(...)->unwrap` 在**导入阶段**执行，所引用的类型必须已经注册。导出时入口脚本被预编译为二进制，其中固化的是编辑器内生成的组件列表，而运行时它先于原生模块与 ToWoo 类型注册执行——这正是入口脚本不能导入 `je::towoo::components` 的原因（参见 `builtin/editor/Editor/Template/runtime_main.wo.template` 与 `builtin/gaming/Saving/World.wo` 中的注释）。

### 脚本系统与脚本组件的注册

脚本系统的注册流程（`je_towoo_system_register`）与运行时包装（`ToWooBaseSystem`、三个阶段与更新器槽位的对应）详见[系统](system.md#towoo-脚本系统的包装)，这里只补充与脚本本身相关的两点：

* 脚本经 `je_file_open` 读入、`wo_load_binary` 编译（源码与预编译二进制均可），随后以 `woort_bootup` 执行一次**顶层代码**——阶段宏与 `system!` / `work!` 展开出的登记调用就在此时把各个工作记入列表，再由 `_init_towoo_system` 交给原生侧按阶段分组；同名旧系统会先被注销，热更新即由此实现。
* 每个系统实例拥有独立的 VM 与上下文，但同一系统类型的所有实例共用这一份代码环境（CodeEnv）。

⚠️ 注意：任一工作函数调用异常中止（如 panic）后，实例的 VM 即被关闭——该系统实例仍留在世界中，但此后不再执行任何更新。另外，脚本全局变量位于代码环境的静态存储，被同一系统类型的**所有实例共享**，而不同世界中的实例是并行执行的，因此实例状态必须放在上下文里。

脚本组件由 `_init_towoo_component(name)`（`je/towoo/component.wo`）把 `component!` 收集到的成员表交给 `wojeapi_towoo_update_component_data`：以 `ToWooBaseComponent` 为头部按对齐计算布局，首次注册时以哈希 `_towoo_component_<名字>` 调用 `je_typing_register`，已注册时改用 `je_typing_reset` 原地更新布局；若同名类型已存在而哈希不同（即原生组件），则 panic 拒绝注册。`woolang` 成员保存为 GC 固定的 Woolang 值。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| ToWooBaseSystem / ToWooBaseComponent、声明生成、系统与组件注册 | `src/jeecs_woolang_towoo_script_basic.cpp` |
| 引擎 API 的 Woolang 绑定 | `src/jeecs_{basic,file,ecs,typeinfo,input,graphic,audio,editor,gui,tilemap}_woolang_api.cpp` |
| `component!` / `system!` / `work!` / 阶段宏 | `je/towoo/component.wo`、`je/towoo/system.wo` |
| 成员句柄、强类型组件访问、数学运算 | `je/towoo/towoo.wo` |
| extern 扫描与 libjoyecs 函数表生成 | `script/prebuild/prebuild.wo`（产物 `src/jeecs_extern_library_woo_api.hpp`） |
| 组件 / 系统脚本模板 | `builtin/editor/Editor/Template/towoo_*.wo.template` |
| C API 与行为注释 | `include/jeecs/typing.hpp`（逐项说明见 [je_towoo_system_register](../capi/typing/je_towoo_system_register.md) 等） |
