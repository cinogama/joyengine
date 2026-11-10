# 变换（Transform）

本文档是[术语表](README.md)中「变换」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述变换相关组件的分工、父子层级，以及世界变换的结算时机与读写方式；
* **第二部分·底层实现**——面向引擎维护者，描述变换结算系统的数据流、逐级结算算法与各阶段的先后关系。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 变换是什么

变换（Transform）描述实体在空间中的位置、旋转与缩放。JoyEngine 没有一个大而全的变换组件，而是把它拆成 `Transform` 命名空间下的一组小组件，按需组合：

| 组件 | 成员 | 作用 |
| --- | --- | --- |
| `LocalPosition` | `pos: vec3` | 局部位置（缺失时视为原点） |
| `LocalRotation` | `rot: quat` | 局部旋转（缺失时视为无旋转） |
| `LocalScale` | `scale: vec3` | 局部缩放（缺失时视为 (1, 1, 1)） |
| `LocalToWorld` | —（内部缓存） | 标记**没有父级**：局部变换直接作为世界变换 |
| `LocalToParent` | `parent_uid` | 标记**有父级**：局部变换相对于 `parent_uid` 所指的父级 |
| `Anchor` | `uid` | 使实体可以作为父级被引用，`uid` 在组件创建时自动生成 |
| `Translation` | 世界位置、世界旋转、缩放、物体到世界矩阵 | 结算结果：实体最终在世界中的变换 |

* `Local*` 三个组件是**输入**，由游戏逻辑或编辑器写入；它们都是可选的，缺失时按默认值参与结算；
* `Translation` 是**输出**，由引擎内置的变换系统每帧根据输入结算，渲染、物理、射线拾取、音频等功能都读取它；
* `LocalToWorld` 与 `LocalToParent` **二选一**，决定实体是层级的根，还是某个父级的子。

拆分的好处是对*切片*友好（见 [slice.md](slice.md)）：只关心位置的系统只需观察 `LocalPosition`，不必为旋转、缩放付出代价；“有没有父级”被编码成组件本身，系统可以直接用 `except` / `contains` 区分根实体与子实体。

### 局部与世界

* **根实体**（带 `LocalToWorld`）：世界变换就是局部变换；
* **子实体**（带 `LocalToParent`）：世界旋转 = 父级世界旋转 × 局部旋转；世界位置 = 父级世界旋转 × 局部位置 + 父级世界位置；
* **缩放不沿层级继承**：子实体的缩放就是它自己的局部缩放，父级的缩放既不影响子级的缩放，也不会放大子级的局部位置。`Translation` 中记录缩放的字段因此名为 `local_scale`。

### 父子层级

父子关系通过 uid 关联，而不是实体句柄（实体句柄不能跨帧持有，见 [entity.md](entity.md)）：

```
父实体：Anchor { uid = U }
子实体：LocalToParent { parent_uid = U }，并且没有 LocalToWorld
```

* 子实体可以再持有 `Anchor`，成为下一级的父级，层级深度不限；
* 编辑器的“挂接实体”操作一次完成全部组件变更：为子实体补 `LocalToParent` 并写入父级的 uid、为父实体补 `Anchor`、移除子实体上的 `LocalToWorld`，同时把子实体的局部位置换算到新父级之下，保持其世界位置不变。

在脚本或原生代码中建立层级，做同样的组件变更即可（组件增删在下一帧生效）：

```woolang
// 把 child 挂到 parent 之下（两个句柄都只在当帧有效）
func attach(child: je::entity, parent: je::entity)
{
    let anchor = parent->towoo::get_component:<Transform::Anchor>
        ->or_do(\= parent->towoo::add_component:<Transform::Anchor>;);
    let l2p = child->towoo::add_component:<Transform::LocalToParent>;
    l2p.parent_uid->set(anchor.uid->get);
    child->towoo::remove_component:<Transform::LocalToWorld>;
}
```

挂接后，子实体原有的局部位置会被解释为相对父级的偏移。若要保持世界位置不变，需要用**父级**的世界变换自行换算：局部位置 = 父级世界旋转⁻¹ × (目标世界位置 − 父级世界位置)。编辑器的挂接操作正是这样做的——挂接刚生效时，子实体自己的 `Translation` 还是按旧层级结算的结果，不能用来反推新父级。

⚠️ 注意：以下实体的 `Translation` **不会被更新**（保持旧值），引擎也不报错——这是有意为之，让问题能被立即发现：

* 同时带有 `LocalToWorld` 与 `LocalToParent`，或两者都没有的实体；
* `parent_uid` 找不到对应 `Anchor` 的实体（父级不存在、已被销毁或摘掉了 Anchor），及其所有后代；
* 层级成环（父级同时是自己的后代）的实体。

### 变换何时被结算

变换结算由内置系统 `Translation::TranslationUpdatingSystem` 完成，它和其他系统一样需要加入世界（编辑器新建世界时会默认添加）。它占用两个更新阶段（见 [system.md](system.md#系统更新的八个阶段)）：

| 阶段 | 所做的事 |
| --- | --- |
| 变换（`TransformUpdate`，第 4 阶段） | 由局部变换与层级结算 `Translation` 的世界位置、世界旋转与缩放 |
| 提交（`CommitUpdate`，第 7 阶段） | 由上述三个分量重建物体到世界矩阵 `object2world` |

由此可知：

* 在 `UPDATE` 中修改局部变换后，**同一帧的 `LATE_UPDATE` 读到的就是新的世界变换**；在 `UPDATE` 中读到的则是上一次结算的结果；
* 物理在第 5 阶段运行，并把刚体的模拟结果写回 `Translation` 与局部变换，所以 `LATE_UPDATE` 读到的是物理之后的位置（见 [physics.md](physics.md)）；
* ⚠️ `object2world` 矩阵到第 7 阶段才更新：在 `LATE_UPDATE` 及更早的阶段读取它，得到的是上一帧的矩阵。逻辑中需要当前位置时，请读取分量。

### 读写世界变换

直接改写 `Translation` 的分量只能维持到下一次变换阶段——届时它会被局部变换重新结算的结果覆盖。要“把实体放到世界中的某个位置/朝向”，应使用 `Translation` 的辅助方法：它们把目标世界值**换算成局部值写入 `LocalPosition` / `LocalRotation`**，同时立即更新 `Translation`，同一帧后续的读取也能看到新值。

| Woolang（`import je::towoo;`） | C++（`Transform::Translation`） | 说明 |
| --- | --- | --- |
| `trans->get_global_pos` / `get_global_rot` / `get_local_scale` | 成员 `world_position` / `world_rotation` / `local_scale` | 读取世界位置 / 世界旋转 / 缩放 |
| `trans->get_parent_pos(lpos, lrot)` / `get_parent_rot(lrot)` | `get_parent_position(...)` / `get_parent_rotation(...)` | 由自身的世界与局部变换反推父级的世界位置 / 旋转 |
| `trans->set_global_pos(pos, lpos, lrot)` | `set_global_position(pos, LocalPosition*, const LocalRotation*)` | 设置世界位置，并换算回写局部位置 |
| `trans->set_global_rot(rot, lrot)` | `set_global_rotation(rot, LocalRotation*)` | 设置世界旋转，并换算回写局部旋转 |

脚本中的 `lpos` / `lrot` 是 `option<...>`（C++ 中为可空指针），实体没有对应的局部组件时传 `option::none`：反推父级时按默认值（原点 / 无旋转）处理；但被设置的那一项若没有局部组件可供回写，就只改动了 `Translation`，下一次结算时会被覆盖。脚本侧的 `Transform::Translation` 不暴露成员字段，只能通过上表的方法访问。

```woolang
UPDATE!
{
    // 改局部变换：变换阶段会据此重新结算 Translation
    system! spin(localrot: Transform::LocalRotation)
    {
        let r = localrot.rot->get;
        localrot.rot->set(r * quat::euler(0., 0., 90. * je::deltatime()));
    }
    // 按世界坐标放置实体：换算并回写 LocalPosition
    system! respawn(
        trans: Transform::Translation,
        localpos: Transform::LocalPosition,
        localrot: Transform::LocalRotation?)
    {
        if (trans->get_global_pos[1] < -100.)
            trans->set_global_pos((0., 5., 0.) as vec3, option::value(localpos), localrot);
    }
}
LATE_UPDATE!
{
    // 已在变换与物理阶段之后：读到的是本帧结算后的世界位置
    system! report(trans: Transform::Translation)
    {
        let (x, y, _) = trans->get_global_pos;
        je::log(F"世界位置：({x}, {y})");
    }
}
```

```cpp
void Update()
{
    for (auto&& [trans, localpos, localrot] : query<view typesof(
        Transform::Translation&, Transform::LocalPosition&, Transform::LocalRotation*)>())
    {
        if (trans.world_position.y < -100.0f)
            trans.set_global_position(math::vec3(0.f, 5.f, 0.f), &localpos, localrot);
    }
}
```

### 相关术语

参见[术语表](README.md)：

* 组件、实体——变换由一组组件构成（[component.md](component.md)、[entity.md](entity.md)）；
* 系统更新——变换结算所处的阶段（[system.md](system.md)）；
* 物理——在变换之后改写刚体变换（[physics.md](physics.md)）；
* 渲染、摄像机——读取物体到世界矩阵（[render.md](render.md)、[camera.md](camera.md)）；
* UI——同样沿 `Anchor` / `LocalToParent` 父链解析布局（[ui.md](ui.md)）；
* 数学概念——向量、四元数与矩阵（[math.md](math.md)）。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。组件定义位于 `include/jeecs/components.hpp` 的 `Transform` 命名空间（源码附有结算流程的 ASCII 图），结算系统位于 `src/jeecs_core_translation_system.cpp`；脚本侧的组件声明与辅助方法见 `je/towoo/components.wo`、`je/towoo/towoo.wo` 与 `src/jeecs_woolang_towoo_script_basic.cpp`。

### 结算数据流

转述源码中的 ASCII 图：

```
LocalPosition ─┐                   ┌─是─→ LocalToParent ──（以父级 Translation 复合）──┐
LocalRotation ─┼─→ 有父级？────────┤                                                  ├─→ Translation
LocalScale    ─┘                   └─否─→ LocalToWorld ─────────────────────────────────┘
```

`LocalToParent` / `LocalToWorld` 都是“两个 vec3 + 一个 quat”（`pos` / `scale` / `rot`）的中间缓存，三个成员在两者中的偏移相同（源码以 `static_assert` 保证）。其中只有 `LocalToParent::parent_uid` 注册为成员，所以脚本与序列化只能看到它，中间缓存不对外暴露。

⚠️ 源码图把 `Translation` 标注为“2x 4x4 matrix”，但当前的结构体只有一个 4×4 矩阵 `object2world`，外加分解后的 `world_position` / `world_rotation` / `local_scale` 三个分量——以结构体定义为准。

### TransformUpdate：逐级结算

`TranslationUpdatingSystem::TransformStageUpdate()` 分三步：

1. **根**：查询 `view<Anchor*, Translation&, LocalToWorld&, LocalPosition*, LocalRotation*, LocalScale*>` 并 `except<LocalToParent>`。把局部值（缺失取默认）拷入 `LocalToWorld`，再写入 `Translation` 的三个分量；带 `Anchor` 的根以 `uid → Translation*` 登记进“已结算表”；
2. **子**：查询 `view<Anchor*, Translation&, LocalToParent&, ...>` 并 `except<LocalToWorld>`。把局部值拷入 `LocalToParent`，放入待处理链表；
3. **逐轮消解**：反复扫描待处理链表，父级 uid 已在表中的项按 `world_rotation = parent.world_rotation * l2p.rot`、`world_position = parent.world_rotation * l2p.pos + parent.world_position`、`local_scale = l2p.scale` 结算；带 `Anchor` 的再登记进表，然后移出链表。某一轮没有任何进展即停止，剩余项保持旧值。

两个查询互相 `except`，所以两种标记组件都有或都没有的实体不会被任何一个查询选中——这就是第一部分那条警告的实现原因。第 k 轮之后，深度不超过 k 的子实体必然已结算，因此扫描轮数不超过“层级深度 + 1”（最后一轮用于确认没有进展）。

同一个 `TransformUpdate()` 中随后执行 UI 布局阶段 `UserInterfaceStageUpdate()`，它沿同样的 `Anchor` + `LocalToParent` 父链解析 UI 元素的布局（见 [ui.md](ui.md)）。

### CommitUpdate：重建矩阵

`CommitUpdate()` 对所有 `Translation` 调用 `math::transform(object2world, world_position, world_rotation, local_scale)`。源码注释写明“到此为止，所有的变换均已应用到 Translation 上”：第 5 阶段的物理、第 6 阶段的 LATE_UPDATE 都可能通过 `set_global_*` 改写分量，矩阵在它们之后统一重建；第 8 阶段的渲染管线与 2D 光照读取 `object2world` 作为模型矩阵。

⚠️ 注意物理回写的时序：物理阶段经 `set_global_position` / `set_global_rotation` 改写的是刚体实体**自身**的 `Translation` 与局部变换；它的子实体已在第 4 阶段按物理之前的父变换结算完毕，本帧不会重新结算，因此会比父刚体滞后一帧，直到下一帧的变换阶段才跟上。

### 辅助方法与 Anchor 的 uid

辅助方法都假设 `Translation` 与局部变换处在一致的结算状态：

* `get_parent_rotation(lrot)` = `world_rotation × lrot⁻¹`（无 `LocalRotation` 时直接返回 `world_rotation`）；
* `get_parent_position(lpos, lrot)` = `world_position − 父级旋转 × lpos`（无 `LocalPosition` 时直接返回 `world_position`）；
* `set_global_position(p, lpos, lrot)`：令 `lpos = 父级旋转⁻¹ × (p − 父级位置)`，再令 `world_position = p`；
* `set_global_rotation(r, lrot)`：令 `lrot = 父级旋转⁻¹ × r`（结算公式“世界 = 父 × 局部”的逆，与 `set_global_position` 同一左乘模式），再令 `world_rotation = r`。

`Anchor` 的 `uid` 由默认成员初始化器 `uid_t::generate()` 生成；它的拷贝构造函数体为空、不复制 uid，移动构造则为默认实现。因此：实体因增删组件在原型间迁移（移动）时 uid 不变，父子关系保持；**依照预制体实例化（拷贝）出的实体会得到新的 uid**，引用预制体 uid 的 `LocalToParent` 不会自动指向实例。世界文件中的 uid 作为注册成员被序列化，载入时恢复。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| `Transform` 组件定义、结算流程 ASCII 图、`Translation` 辅助方法 | `include/jeecs/components.hpp` |
| 变换结算系统（`TransformUpdate` / `CommitUpdate`） | `src/jeecs_core_translation_system.cpp`、`src/jeecs_core_translation_system.hpp` |
| 系统类型注册（`Translation::TranslationUpdatingSystem`） | `src/jeecs_core_systems_and_components.cpp` |
| 脚本侧组件声明 / `Translation` 辅助方法 | `je/towoo/components.wo`、`je/towoo/towoo.wo`、`src/jeecs_woolang_towoo_script_basic.cpp`（`wojeapi_towoo_transform_translation_*`） |
| 物理回写 | `src/jeecs_core_physics_system.cpp`（`phase_sync_bodies_out`） |
| 编辑器的挂接操作 | `builtin/editor/Editor/Action/AttachEntity.wo`、`src/jeecs_ecs_woolang_api.cpp`（`wojeapi_set_parent`） |
| 编辑器新建世界时默认添加的系统 | `builtin/editor/Editor/WorldEditorViewport/WorldOpen.wo` |
