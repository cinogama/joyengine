# 切片（Slice）

本文档是[术语表](README.md)中「切片」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述切片的过滤条件与在 Woolang / C++ 中的声明方式；
* **第二部分·底层实现**——面向引擎维护者，描述切片在引擎内部是如何收集需求、圈定原型并直接寻址组件的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 切片是什么

切片（Slice）是按组件类型过滤实体得到的子集。*系统*（包括渲染系统）从不“持有”实体，而是每帧通过切片定位自己感兴趣的实体，再读写它们的组件——切片就是系统观察世界的窗口。

一个切片由两部分构成：

* **视图**：要取出哪些组件交给系统读写；
* **过滤条件**：实体必须满足的组件构成要求。

切片只会选中**正常生效**的实体：

* [预制体](prefab.md)永远不会被选中；
* 本帧刚创建的实体、本帧刚添加的组件，要到下一帧才出现在切片中；
* 本帧被销毁的实体、本帧被移除的组件，在本帧剩余时间里**仍然会**出现在切片中。

这与[世界·结构变更的生效时机](world.md#结构变更的生效时机)一致：结构变更统一在下一帧开始时落地。此外，切片的遍历顺序取决于实体在存储中的位置，而存储位置会随碎片整理变化，**不要依赖遍历顺序**。

### 四种过滤条件

| 条件 | 语义 | C++ 写法 | Woolang `system!` 写法 |
| --- | --- | --- | --- |
| 必须包含 | 实体必须拥有该组件 | 视图中的 `T&`，或 `contains<T>` | 参数 `name: T`，或子句 `contains T;` |
| 可有可无 | 不影响筛选；有则取出，没有则为空 | 视图中的 `T*`（缺少时为 `nullptr`） | 参数 `name: T?`（类型为 `option<T>`） |
| 多选一 | 至少拥有组内的一个组件 | `anyof<A, B>` | 子句 `anyof A, B;` |
| 排除 | 实体不能拥有该组件 | `except<T>` | 子句 `except T;` |

几点补充：

* 出现在视图中的组件会被取出；只写在 `contains` 里的组件只参与筛选、不会被取出；
* 可以同时写多个“多选一”组，**每一组都必须满足**；
* “可有可无”常与“多选一”搭配使用：用多选一保证至少有一个，再用可有可无把它们都取出来、看看到底有哪个。引擎收集摄像机时就是这样写的——`OrthoProjection*` 与 `PerspectiveProjection*` 都放进视图，再要求 `anyof<OrthoProjection, PerspectiveProjection>`。

### 在 Woolang 系统中声明切片

`system!` 宏的参数列表就是视图，参数列表之后、函数体之前可以写 `contains` / `except` / `anyof` 子句（每条以 `;` 结尾）。函数体内除了各个组件参数，还可以使用系统上下文 `context` 与当前实体 `e`：

```woolang
UPDATE!
{
    system! move_up(
        localpos: Transform::LocalPosition,
        tint: Renderer::Color?
    )
    contains Transform::LocalToWorld;   // 只处理没有父实体的实体
    {
        let p = localpos.pos->get;
        localpos.pos->set((p[0], p[1] + je::deltatime(), p[2]) as vec3);

        match (tint)
        {
            value(c)? c.color->set((1., 1., 1., 1.) as color);
            none?;                          // 实体没有 Renderer::Color
        }
    }
}
```

切片的结果由引擎缓存并在原型集合变化时自动刷新，使用者无需关心。

⚠️ 注意 当前版本中，`system!` 的 `except` 与 `anyof` 子句存在已知的枚举错位问题，实际效果与上表相反（详见[第二部分·脚本侧需求枚举错位](#脚本侧需求枚举错位)）。在问题修复前，请避免在脚本系统中依赖这两种子句；C++ 系统与下文的 `entity_selector` 不受影响。

### 在系统之外：entity_selector

不在系统里、但也需要按组件筛选实体时（例如编辑器工具），可以创建一个持久化的选择器（`world::create_selector`）。它支持“必须包含 / 排除 / 多选一”三种条件，每个 `anyof(...)` 各成一组；选择器没有视图，遍历得到的是实体，需要组件时再自行 `get_component`：

```woolang
let selector = w->create_selector([
    requirement::contains(Transform::Translation::type::typeinfo),
    requirement::except(Editor::Invisible::type::typeinfo),
    requirement::anyof([
        Renderer::Shape::type::typeinfo,
        UserInterface::Text::type::typeinfo,
    ]),
]);

for (let e : selector)
{
    // e 是本帧有效的实体句柄，需要组件时自行获取
    let trans = e->towoo::get_component:<Transform::Translation>()->unwrap;
    je::log(trans->get_global_pos);
}
```

选择器可以长期保存、反复遍历：每次开始遍历时，它会检查世界的原型集合是否变化，必要时自动刷新缓存，这一过程对使用者透明。遍历得到的实体句柄依然只在当帧有效。

### 在 C++ 系统中遍历切片

C++ 系统在成员函数中调用 `query<视图, 条件...>()`，得到一个可以用范围 for 遍历的切片，每个元素是组件引用/指针构成的元组；`query_entity` 额外在元组最前面附上实体句柄。引擎代码中常用 `typesof(...)` 宏（展开为 `<...>`）书写长参数列表，下例使用等价的普通写法：

```cpp
using namespace jeecs;
using namespace jeecs::slice_requirement;

void MySystem::Update()
{
    for (auto&& [trans, shape, color] : query<
        view<Transform::Translation&, Renderer::Shape&, Renderer::Color*>,
        except<Transform::LocalToParent>>())
    {
        if (color != nullptr)
            color->color.a = 1.f;
    }

    for (auto&& [e, hp] : query_entity<view<MyGame::Health&>>())
    {
        if (hp.hp <= 0)
            e.close();
    }
}
```

切片还提供 `foreach_parallel(func)` 以并行方式遍历；它是前向迭代器，直接交给 `std::for_each` 的并行策略时多数标准库难以有效切分工作。

### 相关术语

参见[术语表](README.md)：

* [系统](system.md)——切片的主要使用者，以及系统更新的执行阶段；
* [实体](entity.md)、[组件](component.md)——切片筛选的对象与取出的数据；
* [预制体](prefab.md)——不会被切片选中的模板实体；
* [世界](world.md)——切片以世界为单位进行查询；
* [渲染](render.md)——渲染系统同样通过切片收集待渲染的实体与摄像机。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。需求收集、原型圈定与切片缓存实现于 `src/jeecs_basic_arch_system.cpp`；C ABI 与需求结构定义于 `include/jeecs/ecs.hpp`；C++ 切片封装位于 `include/jeecs/game.hpp`（`slice_requirement` / `collection`）；towoo 系统的遍历位于 `src/jeecs_woolang_towoo_script_basic.cpp`；`entity_selector` 位于 `src/jeecs_ecs_woolang_api.cpp`。

### 需求的表示

每条需求是一个 `je_component_requirement_t { m_kind, m_typeid }`，`m_kind` 取自 `je_component_requirement_kind_t`：`CONTAINS`、`MAYNOT`、`EXCEPT`、`ANYOF_0`。多选一的分组直接编码在 `m_kind` 中：第 `g` 组记为 `JE_COMPONENT_REQUIRE_ANYOF_0 + g`。各入口的分组方式：C++ `collection` 从 1 开始、每个附加需求参数递增一次；`entity_selector` 每个 `anyof(...)` 一组；towoo 系统由 `m_require_group` 计数。

一组需求中，**视图需求必须排在最前**：`je_ecs_requirements_collect(requirements, view_count, other_count)` 把前 `view_count` 条视为视图需求，其余为附加需求。C++ 视图中的引用类型生成 `CONTAINS`、指针类型生成 `MAYNOT`；towoo 的参数按声明顺序构成视图；`entity_selector` 没有视图（`view_count` 为 0）。

### 需求收集

`je_ecs_requirements_collect` 产出不透明的 `je_collected_requirements_t`：

* `m_contain_set` / `m_except_set`——排序去重的类型集合；
* `m_anyof_sets`——`map<分组, 类型集合>`；
* `m_view_required`——视图需求的副本（决定取出哪些组件、按什么顺序）；
* `m_selected_archs` 与 `m_selected_arch_cache`——已命中的原型，以及每个原型的视图偏移缓存。

视图需求同样参与集合的构建（`CONTAINS` 进入包含集合）；`MAYNOT` 不进入任何集合，只影响视图。

### 按原型圈定

`ArchManager::update_collection_archinfo` 遍历世界的全部原型，命中条件为：类型集合包含 `m_contain_set`、与 `m_except_set` 无交集、且与每个多选一组都有交集。命中的原型调用 `create_chunk_info`，为每个视图组件记录其列在 chunk 中的起始偏移与组件尺寸；原型中没有该组件时记为 `0 / 0`（断言只有非 `CONTAINS` 的需求允许出现这种情况）。

刷新是**增量**的：已在 `m_selected_arch_cache` 中的原型直接跳过。原型在世界存续期间从不销毁，因此命中集合只增不减。结果写回 `je_requirement_collection_t` 的 `m_cached_archs` / `m_cached_arch_count`；其中每项的 `m_entity_count` 是**单个 chunk 的槽位容量**，并非实体数量。

### 遍历与直接寻址

遍历沿“原型 → chunk 链表 → 槽位”三层进行：`je_arch_chunk_get` 取首个 chunk，`je_arch_chunk_next` 走链表，`je_arch_entity_meta_addr_in_chunk` 取槽位元信息，**只接受 `JE_ENTITY_STAT_READY`**。组件地址直接由 `chunk 地址 + 列偏移 + 组件尺寸 × 槽位` 算出（chunk 缓冲位于 chunk 对象起始处），不做任何查找。

C++ 侧的 `slice_base` 在进入新原型时把视图偏移预缓存到 `m_cached_infos`，避免每次解引用都走两层间接；`get_component_from_archchunk` 以 `typing::pack_index_v` 在编译期确定组件在视图中的下标。引用类型（`CONTAINS`）走**无分支路径**，只以断言校验尺寸；指针类型（`MAYNOT`）检查单位尺寸是否为 0，为 0 即返回 `nullptr`。`entity_slice` 另从元信息中取出版本号，组成本帧有效的 `game_entity`。

towoo 系统在 `ToWooBaseSystem::_invoke_multi_work` 中做同样的遍历：为每个 `READY` 实体构造组件结构体（`CONTAINS` 直接构造，`MAYNOT` 包装为 `option`），连同新建的实体句柄一起压栈调用脚本函数。脚本函数出错时立即中止遍历并关闭该系统实例的虚拟机，此后这个系统实例不再执行任何工作。

### 切片缓存

* **系统的切片**缓存在世界中，按（系统实例，切片哈希）两级索引：C++ 的切片哈希是 `typeid(collection<视图, 条件...>).hash_code()`，towoo 则是该系统内部的工作序号（只要求在同一系统实例内唯一）。首次查询时 `je_ecs_world_slice_dependence_query` 返回 `false`，由调用方填充需求并立即调用 `je_ecs_world_collection_update`，因此**首次查询当帧即可使用**；新缓存经命令缓冲登记，此后由 `World::update()` 在原型集合变化时统一刷新。按系统实例分组的原因、刷新时机与生命周期详见[世界·切片缓存与编译防火墙](world.md#切片缓存与编译防火墙)。
* **`entity_selector`** 自行持有一份 `je_requirement_collection_t`，不挂在任何系统上。创建时刷新一次并记下 `je_ecs_world_arch_change_version_get` 的值；每次 `iter()` 比较版本号，变化时才调用 `je_ecs_world_collection_update`，所以单次 `for` 循环内看到的原型集合是一致的。

⚠️ 注意 刷新会向 `m_selected_archs` 追加元素，向量扩容后 `m_cached_archs` 指向的数组可能整体搬迁。C ABI 注释明确“获取到的查询缓存仅在当前帧有效，不应当被持久化保存”——不要跨帧保存切片迭代器或缓存数组的指针。

### 脚本侧需求枚举错位

⚠️ 注意 towoo 的需求种类在 `je/towoo/system.wo` 中定义为 `enum require_type { CONTAINS, MAYNOT, ANYOF, EXCEPT }`（即 `ANYOF = 2`、`EXCEPT = 3`），而 C 枚举 `je_component_requirement_kind_t` 的顺序是 `CONTAINS, MAYNOT, EXCEPT, ANYOF_0`（`EXCEPT = 2`、`ANYOF_0 = 3`）。`wojeapi_towoo_register_system_job` 把脚本传入的整数直接转换为 C 枚举，于是：

* `except T;` 被当作只含 `T` 的多选一组——效果等同于**必须包含** `T`；
* `anyof A, B;` 中的每个类型都被当作**排除**条件——效果是 `A`、`B` 都不能有。

视图参数只用到 `CONTAINS` / `MAYNOT`，两侧取值一致，不受影响；`entity_selector` 在原生侧显式映射每种需求，C++ 的 `slice_requirement` 直接使用 C 枚举，也都不受影响。修复时应让两侧枚举顺序保持一致，或在 `wojeapi_towoo_register_system_job` 中做显式映射。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 需求结构、种类枚举、`je_ecs_requirements_collect` 等 C ABI 与注释 | `include/jeecs/ecs.hpp` |
| 需求收集、原型圈定、切片缓存 | `src/jeecs_basic_arch_system.cpp`（`je_ecs_requirements_collect`、`ArchManager::update_collection_archinfo`、`ArchType::create_chunk_info`、`World::fetch_and_request_slice_cache_dependence`） |
| C++ 切片（`view` / `contains` / `except` / `anyof`、`collection`、`query`） | `include/jeecs/game.hpp` |
| `system!` 宏与 towoo 需求声明 | `je/towoo/system.wo` |
| towoo 需求登记与遍历 | `src/jeecs_woolang_towoo_script_basic.cpp`（`wojeapi_towoo_register_system_job`、`ToWooBaseSystem::_invoke_multi_work`） |
| `entity_selector` | `je/ecs.wo`；原生侧 `src/jeecs_ecs_woolang_api.cpp` |
| 引擎内切片用例（摄像机收集等） | `src/jeecs_core_graphic_system.cpp`、`src/jeecs_core_translation_system.cpp` |
