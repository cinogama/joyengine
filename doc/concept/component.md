# 组件（Component）

本文档是[术语表](README.md)中「组件」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述组件的定位、声明方式与成员访问；
* **第二部分·底层实现**——面向引擎维护者，描述类型注册、脚本组件与组件存储在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 组件是什么

组件（Component）是纯粹的*数据*：一组成员字段，本身不包含游戏逻辑。逻辑写在*系统*里——系统通过*切片*找到带有特定组件的实体，读取或修改组件的成员。

实际开发中允许组件带一些简单的、访问器性质的方法。例如 C++ 的 `Transform::Translation` 提供 `set_global_position` 等换算方法，Woolang 侧也为它扩展了 `get_global_pos` 等函数——它们只是对数据的便捷读写，不承担游戏逻辑。

组件以**类型**区分：每个组件类型都在引擎的类型系统中注册，并拥有唯一的名字（如 `Transform::LocalPosition`）。一个实体对同一组件类型**最多持有一个实例**；实体本身不过是这些组件实例的集合（见[实体](entity.md)）。

### 内置组件族

引擎内置的组件按命名空间分族，各自的用法见对应文档：

| 命名空间 | 内容 | 详见 |
| --- | --- | --- |
| `Transform` | 位置、旋转、缩放与父子层级 | [变换](transform.md) |
| `UserInterface` | UI 元素的布局与文本 | [UI](ui.md) |
| `Renderer` | 形状、着色器、纹理、颜色、渲染队列 | [渲染](render.md) |
| `Camera` | 投影、视口、清屏、渲染到帧缓冲 | [摄像机](camera.md) |
| `Light2D` | 2D 光源与阴影 | [光照](light.md) |
| `Physics2D` | 刚体、碰撞体与物理参数 | [物理](physics.md) |
| `Animation` | 动画播放器（`Animator`） | [动画](animation.md) |
| `Particle` | 粒子发射器（`Emitter`） | [粒子](particle.md) |
| `Tilemap` | 瓦片地图 | [瓦片地图](tilemap.md) |
| `Audio` | 声源、监听者与播放状态 | [音频](audio.md) |
| `Input` | 虚拟手柄（`VirtualGamepad`） | [输入](input.md) |

此外还有供编辑器使用的 `Editor::*` 组件（如保存实体名的 `Editor::Name`、标记不显示也不保存的 `Editor::Invisible`）。

### 用 Woolang 声明脚本组件

脚本组件写在名为 `<组件名>.je4component.wo` 的文件里：**文件名去掉后缀就是组件名**。编辑器会自动发现项目中的组件脚本并完成注册，新建文件时也可直接使用资源管理器里的“ToWoo 组件”模板。组件成员用 `component!` 宏声明：

```woolang
// Health.je4component.wo —— 组件名即 Health
import pkg::std;

import je::towoo::component;
import je::towoo::types;

component!
{
    hp: int32,
    max_hp: int32,
    spawn_point: vec3,
}
```

* 普通成员写作 `名字: 类型`。类型必须是脚本可识别的类型：`int8` 至 `int64`、`uint8` 至 `uint64`、`float`、`real`、`bool`、`string`、`vec2`/`vec3`/`vec4`、`ivec2`、`quat`、`color`、`uuid` 等。注意**没有 `int`**，整数成员请使用 `int32` 这类定宽类型；
* *Woolang 值成员*写作 `名字: woolang 类型 = 初值`（如 `title: woolang string = "player"`），可以保存任意 Woolang 值，每个新组件实例都以该初值初始化（当前版本此特性存在已知问题，见[第二部分·注册层与成员反射](#注册层与成员反射)）；
* 脚本组件**不能与原生组件同名**，否则注册失败；
* 组件注册（或修改）后，引擎会重新生成脚本侧的组件声明，系统脚本随后就能以 `Health` 这个名字引用它。

### 访问组件成员

Woolang 侧（towoo）拿到的组件是一个结构体，每个成员都是一个指向组件内存的*成员句柄*：`->get` 读出成员值的**一份拷贝**，`->set(v)` 把新值写回组件。修改 `get` 得到的值不会影响组件，必须调用 `set`：

```woolang
UPDATE!
{
    system! regen(h: Health)
    {
        let v = h.hp->get;          // 读出拷贝
        if (v < h.max_hp->get)
            h.hp->set(v + 1);       // 写回组件
    }
}
```

从实体上按类型取组件用 `e->towoo::get_component:<Health>()`（返回 `option<Health>`），详见[实体](entity.md)。C++ 侧组件就是普通结构体，直接读写字段即可。

读写成员数据是**即时生效**的，不经过命令缓冲（与增删组件这类结构变更不同，见[世界·结构变更的生效时机](world.md#结构变更的生效时机)）。但要注意：组件在世界中的存储位置会随实体的组件变化和碎片整理而移动，因此**组件指针、Woolang 组件结构体与成员句柄都只在当前帧内有效**，不要跨帧保存。

### 用 C++ 声明组件

C++ 组件是一个结构体：删除赋值运算符、保留默认/拷贝/移动构造，并在静态函数 `JERefRegsiter` 中登记需要反射（供编辑器与脚本访问）的成员：

```cpp
#include "jeecs/typing.hpp"

namespace MyGame
{
    struct Health
    {
        JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Health);

        Health() = default;
        Health(const Health&) = default;
        Health(Health&&) = default;

        int32_t hp = 100;

        static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
        {
            jeecs::typing::register_member(guard, &Health::hp, "hp");
        }
    };
}

// 在模块的类型注册函数中（编辑器生成的 loader.hpp）：
jeecs::typing::register_type<MyGame::Health>(guard, "MyGame::Health");
```

组件**必须可拷贝构造、可移动构造**：实体增删组件或碎片整理时引擎会移动组件，依照[预制体](prefab.md)实例化时会拷贝组件。注意函数名就是 `JERefRegsiter`（源码中的拼写），写成别的名字不会报错，只是成员不会被登记。

### API 速览

#### Woolang（`je::typeinfo` / `je::towoo`）

| 接口 | 说明 |
| --- | --- |
| `T::type::typeinfo` | 生成的组件声明中，每个组件类型附带的类型信息常量 |
| `typeinfo::load(name)` | 按名字查找类型信息 |
| `typeinfo::get_all_components_types()` | 列出全部已注册的组件类型 |
| `comp.member->get` / `comp.member->set(v)` | 读 / 写 towoo 组件成员 |
| `component->get_member(name)` / `get_members()` | 按名字反射访问成员（编辑器、序列化使用） |
| `towoo::update_component(name, path)` | 编译并注册脚本组件（编辑器自动调用） |

#### C++（`jeecs::typing`，头文件 `jeecs/typing.hpp`）

| 接口 | 说明 |
| --- | --- |
| `typing::register_type<T>(guard, "Ns::Name")` | 注册组件类型（名字为空则视为基础类型） |
| `typing::register_member(guard, &T::field, "field")` | 在 `JERefRegsiter` 中登记成员 |
| `typing::id<T>()` / `typing::of<T>()` | 取类型 ID / 类型信息 |
| `entity.get_component<T>()` | 从实体取组件指针（见[实体](entity.md)） |

### 相关术语

参见[术语表](README.md)：

* [实体](entity.md)——组件实例的集合；
* [系统](system.md)、[切片](slice.md)——读写组件的逻辑，以及它们定位组件的方式；
* [预制体](prefab.md)——以组件值为模板批量实例化实体；
* [脚本](script.md)——`component!` / `system!` 等 towoo 宏的整体介绍。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。类型注册表位于 `src/jeecs_basic_type_mgr.cpp`，C++ 注册层位于 `include/jeecs/typing.hpp`，脚本组件与 towoo 声明生成位于 `src/jeecs_woolang_towoo_script_basic.cpp`，组件的存储位于 `src/jeecs_basic_arch_system.cpp`。

### 类型注册表

所有类型（组件、系统、基础类型）都登记在全局单例 `FactoryHolder` 中，用 `je_type_info_t` 描述：ID、名字、尺寸与对齐、类型哈希、构造/析构/拷贝/移动四个函数、类别 `je_typing_class_t`（`JE_BASIC_TYPE` / `JE_COMPONENT` / `JE_SYSTEM`），以及可选的成员链表、脚本解析器与系统更新器。

`je_typing_register` 的规则：

* **名字是唯一标识**。类型 ID 是登记表中的 1 基下标，注销后空出的槽位会被复用；
* 同名类型被再次注册（例如每个模块都会为内置组件注册一份自己的本地类型信息）时，**沿用同一个 ID**，新的 `je_type_info_t` 以 `m_next` 链挂在已有类型之后。成员与脚本解析器的查找（`find_member_by_name` / `get_script_parser`）在链头缺失时沿 `m_next` 继续；
* 另维护“哈希 → ID”表，哈希冲突只记录错误日志。按名字查找失败时，还会退而按脚本类型名（`m_woolang_typename`）匹配。

C++ 侧的类型哈希是 `typeid(T).hash_code()`（`basic::type_hash<T>()`），`typing::of<T>()` 即以它查表。`register_type<T>` 传入空名字时以 `typeid(T).name()` 命名并登记为 `JE_BASIC_TYPE`；否则派生自 `game_system` 的登记为 `JE_SYSTEM`，其余登记为 `JE_COMPONENT`。

### 注册层与成员反射

`type_unregister_guard` 记录本模块注册过的类型（哈希 → ID → 本地类型信息），模块退出时 `unregister_all_types` 统一注销，贯彻“谁注册谁释放”。`register_type<T>` 在**首次**注册时检测 `T::JERefRegsiter`，存在则调用它；`register_member` 会先把成员类型自动注册为基础类型（断言其类别为 `JE_BASIC_TYPE`），再以成员指针换算出的偏移调用 `je_typing_member_register`，登记到本模块的本地类型信息上。若 `T` 提供静态的 `JEScriptTypeName` / `JEScriptTypeDeclare` 与 `JEParseFromScriptType` / `JEParseToScriptType`，还会登记脚本解析器。

`default_functions<T>` 生成四个生命周期函数：构造函数依次尝试 `T(void*, const je_type_info_t*)`、`T(void*)`、`T(je_game_world_t*, ...)`、`T(const je_type_info_t*)`、`T{}` 等签名（组件构造时参数为空，系统构造时传入世界）；类型不可拷贝/移动构造时，拷贝器/移动器**只记录错误日志**，目标内存保持未构造状态。

⚠️ 注意 上述能力都靠 C++ concept 检测，**检测失败是静默的**：`JERefRegsiter` 拼错，成员就一个都不会登记；`JEScriptTypeName` / `JEScriptTypeDeclare` 不是 `static`，`has_JEScriptTypeInterface` 就不成立，类型拿不到脚本解析器。

### 脚本组件的注册

`towoo::update_component(name, path)` 编译组件脚本并完成引导，然后调用脚本中的 `_init_towoo_component(name)`；它把 `component!` 宏收集到的成员列表交给原生函数 `wojeapi_towoo_update_component_data`：

1. 若同名类型已存在但哈希不等于 `hash_compile_time("_towoo_component_" + name)`，说明与原生组件重名，直接 panic；
2. 计算布局：以 `ToWooBaseComponent`（只有一个指向自身类型信息的 `m_type` 指针）为头部，成员按各自对齐依次排布，总尺寸按最大对齐取整；
3. 新组件走 `je_typing_register`（类别 `JE_COMPONENT`，四个生命周期函数均来自 `ToWooBaseComponent`）；已存在则走 `je_typing_reset` 原地改写尺寸、对齐与函数并清空成员；
4. 逐个 `je_typing_member_register` 登记成员，Woolang 值成员额外记录脚本类型名与固定（pin）住的初值。

`ToWooBaseComponent` 的构造、拷贝、移动、析构都按成员链表逐个委托给成员类型的对应函数；Woolang 值成员（`PinValue`）在构造后从初值复制一份。

⚠️ 注意 `je_typing_reset` 原地修改了类型信息，而已存在的原型是按旧尺寸计算的列偏移。因此**脚本组件的重载必须在没有该组件实例存活时进行**——编辑器的做法是先冻结全部世界并保存快照，更新完成后再用快照重建世界（`builtin/editor/Editor/WorldEditorViewport/FreezeWorlds.wo`）。

### towoo 声明文件的生成

`je_towoo_api_update` 在每次 `entry::module_entry` 末尾执行，编辑器在注册/更新脚本组件后也会经 `towoo::update_api()` 调用它。它遍历全部已注册类型，生成两个文件并写入 Woolang 虚拟文件系统：

* `je/towoo/types.wo`——每个带脚本解析器的类型（按脚本类型名去重）输出其类型声明，以及 `namespace X { using type = void { public let typeinfo = ...; } }`；
* `je/towoo/components.wo`——每个 `JE_COMPONENT` 类型按名字最后一个 `::` 拆出命名空间，输出 `struct{ __addr: handle, 成员: je::towoo::member<值类型, 解析类型::type>, ... }` 及其 `type::typeinfo`。

桌面平台的 Debug 构建还会把两个文件落盘到源码树的 `je/towoo/` 下，供 IDE 补全；它们已被 `je/towoo/.gitignore` 忽略，不纳入版本管理，也不应手工修改。

运行时，系统函数参数、`towoo::get_component` 等返回的组件结构体由 `create_component_struct` 构造：字段 0 是组件地址，字段 `i+1` 是第 `i` 个**有脚本解析器的**成员的地址（与 `components.wo` 的生成规则一致，没有解析器的成员在两侧都被跳过）。成员句柄的 `get` / `set` 进入 `wojeapi_towoo_member_get` / `_set`，用成员类型的脚本解析器在该地址上做 C++ ↔ Woolang 转换。

### 组件在 chunk 中的布局

组件构成相同的实体归入同一个 `ArchType`。原型构造时把组件类型按尺寸升序排列，用 16KB 的 `CHUNK_SIZE` 估算每个 chunk 能容纳的实体数，再为每个组件类型分配一段按其对齐取整的**连续列**：同一 chunk 内，同类型组件紧密排列在一起（按类型分列，即 SoA）。chunk 缓冲位于 `arch_chunk` 对象的起始处，因此组件地址可直接由 `chunk 地址 + 列偏移 + 组件尺寸 × 槽位` 算出——这正是切片能直接寻址的基础（见[切片](slice.md)）。实体元信息（版本号与状态）另存于每个 chunk 的独立数组中。

组件实例在存储中的生命周期：创建实体时就地构造；增删组件导致实体换原型、或碎片整理搬动实体时，先移动构造到新位置再析构旧实例；依照预制体实例化时拷贝构造；移除组件或销毁实体时析构。`add_component` 返回的新实例先在命令缓冲中单独分配，下一帧才移动进 chunk（详见[实体](entity.md)）。原型与 chunk 的整体管理见[世界·原型与分块存储](world.md#原型与分块存储)。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 类型信息结构、`je_typing_*` C ABI | `include/jeecs/typing.hpp` |
| 类型注册表 `FactoryHolder` | `src/jeecs_basic_type_mgr.cpp` |
| C++ 注册层（`register_type` / `register_member` / guard） | `include/jeecs/typing.hpp`（`jeecs::typing`） |
| 内置组件定义与注册 | `include/jeecs/components.hpp`、`include/jeecs/entry.hpp` |
| 编辑器组件（`Editor::*`） | `src/jeecs_core_editor_components.hpp` |
| 脚本组件、towoo 声明生成、成员 get/set | `src/jeecs_woolang_towoo_script_basic.cpp` |
| `component!` 宏 | `je/towoo/component.wo` |
| 成员句柄 `je::towoo::member` | `je/towoo/towoo.wo` |
| 组件存储（`ArchType` / `arch_chunk`） | `src/jeecs_basic_arch_system.cpp` |
