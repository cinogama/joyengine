# 物理（Physics）

本文档是[术语表](README.md)中「物理」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述 2D 物理的组件约定与用法；
* **第二部分·底层实现**——面向引擎维护者，描述上述行为在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 物理是什么

物理（Physics）让实体像现实中的物体一样受力、碰撞、摩擦和反弹。JoyEngine 通过 Box2D（v3.1.1）模拟 **2D** 物理：*物理系统*（类型名 `Physics::Physics2DUpdatingSystem`）每帧读取物理组件、推进模拟，再把结果写回实体的[变换](transform.md)。

物理系统和其它[系统](system.md)一样需要注册进[世界](world.md)才会工作。在编辑器中通过系统列表添加；放进“调试时”分组的系统只在调试运行时才加入世界，编辑场景期间不会模拟。

### 物理场景与配置脚本

每个世界需要**恰好一个**挂有 `Physics2D::Scene` 组件的实体，其成员 `physics_config` 指向一份*物理配置脚本*（`.je4physics.wo`，可在编辑器资源浏览器中“新建→物理世界配置”生成）。这份脚本是该世界物理设置的唯一来源；世界中没有 Scene 实体时，所有物理模拟都会被拆除；存在多个时只使用第一个并报错。运行期间只有 `physics_config` 指向的路径变化时才会重新加载脚本。

配置脚本用 `je::physics2d::config` 提供的三个宏声明*层*、*碰撞组*与*碰撞关系*，编辑器生成的模板如下：

```woolang
import je::physics2d::config;

// 声明一个物理层，不同层的实体不会发生碰撞
PHYSICS2D_LAYER! 0
{
    gravity: (0.0, -9.8);
    substeps: 4;
    sleep: true;
    continuous: false;
}

// 碰撞组：满足指定组件特征的实体构成一个组（空列表匹配所有实体）
PHYSICS2D_GROUP! 默认组
{
}

// 碰撞关系：每行列出的组两两之间可以碰撞
PHYSICS2D_COLLISIONS!
{
    默认组, 默认组;
}
```

* **层**：每个层是一个独立的物理世界，层号与刚体的 `Rigidbody::layerid` 对应。字段均可省略，缺省值为 `gravity: (0.0, -9.8)`、`substeps: 4`（求解子步数）、`sleep: true`（允许刚体休眠）、`continuous: false`（全局连续碰撞检测）。
* **碰撞组**：花括号内逐项列出组件类型（以 `;` 结尾），默认是“必须包含”，前缀 `except` 表示“必须不含”。一个实体可以同时属于多个组；最多声明 **16** 个组，所有层共用同一张组表。
* **碰撞关系**：每行（以 `;` 结尾）列出的组两两可碰撞；组要与自身碰撞，需在同一行写两次（如模板中的 `默认组, 默认组;`）。

⚠️ 注意 **不属于任何碰撞组的刚体不会与任何物体碰撞**；只声明了组、没有在碰撞关系中配对，同样不会碰撞。

### 刚体：参与模拟的组件集

一个实体要参与物理模拟，至少需要：

* `Transform::Translation`，以及用于写回结果的 `Transform::LocalPosition`、`Transform::LocalRotation`（编辑器“一般变换”预设已包含）；
* `Physics2D::Rigidbody`——刚体本体，`layerid` 指定所属的层；
* **恰好一种**碰撞体：`Collider::Box` / `Circle` / `Capsule` / `Mesh`。

刚体类型由标记组件决定（至多一个）：

| 组件 | 刚体类型 | 说明 |
| --- | --- | --- |
| （无） | 静态 | 不运动，供地面、墙壁等使用 |
| `DynamicBody` | 动态 | 受重力与碰撞影响，由模拟驱动 |
| `KinematicBody` | 运动学 | 不受力，按速度运动 |

同时挂 `DynamicBody` 与 `KinematicBody` 时按动态处理并报错。`Bullet` 标记为高速物体开启单体连续碰撞检测，防止穿透。编辑器的“添加组件→2D物理”提供了静态刚体、动力学刚体、运动学刚体、触发器等常用组合。

### 碰撞体与材质

| 碰撞体 | 成员（默认值） | 说明 |
| --- | --- | --- |
| `Collider::Box` | `size`（1, 1） | 矩形，`size` 为全宽高 |
| `Collider::Circle` | `radius`（0.5） | 圆形 |
| `Collider::Capsule` | `radius`（0.25）、`height`（1） | 沿 Y 轴的胶囊，`height` 为总高；总高不超过直径时退化为圆 |
| `Collider::Mesh` | `vertices`（单位正方形） | 任意简单多边形（凹凸均可、绕序任意），自动分解为凸块 |

碰撞体尺寸直接以世界单位解释，**物理系统不读取实体的缩放**。`Offset::Position` / `Offset::Rotation`（角度）为刚体相对实体的局部偏移。

材质由三个可选组件描述：`Density`（密度，默认 1）、`Friction`（摩擦，默认 0.2）、`Restitution`（弹性，默认 0）。

⚠️ 注意 括号中是**组件被添加时**的默认值；**实体没有该组件时按 0 处理**。尤其是没有 `Density` 的动态刚体质量为零，Box2D 不会对它施加重力——编辑器的“动力学刚体”预设因此自带 `Density` 与 `Friction`。

`IsTrigger` 把碰撞体变为*触发器*（传感器）：不产生碰撞响应。⚠️ 注意 当前实现中触发器的重叠**不会**写入 `CollisionResult`（原因见第二部分）。

### 运动状态与自由度锁定

* `LinearVelocity`（vec2）/ `AngularVelocity`（弧度/秒）是**双向**的：物理系统每帧把模拟后的速度写回组件；脚本写入新值后，下一次物理更新会把它设置给刚体。
* `LinearDamping`、`AngularDamping`、`GravityScale`（默认 1）是**单向**的，只从组件同步到刚体。
* `LockRotation` 禁止旋转；`LockTranslationX` / `LockTranslationY` 使对应轴的位置不被写回，该轴的线速度读数为 0。

### 物理与变换的关系

物理模拟在系统更新的物理阶段执行：**晚于变换结算、早于 `LATE_UPDATE!`**（阶段顺序见[世界](world.md#系统更新的八个阶段)）。每帧的数据流向是：

1. 读入：实体的世界位姿（叠加偏移）与刚体当前位姿不一致时，刚体被**瞬移**到实体位置——在 `UPDATE!` 中直接改 `LocalPosition` 就是瞬移；
2. 推进模拟；
3. 写回：把刚体的位置（x、y）与绕 Z 轴的旋转写回 `Translation`，并反算 `LocalPosition` / `LocalRotation`；z 坐标保持不变。

因此 `LATE_UPDATE!` 读到的是本帧的模拟结果，`PRE_UPDATE!` / `UPDATE!` 读到的是上一帧的。

### 读取碰撞结果

需要接触信息的实体请**自行添加** `Physics2D::CollisionResult` 组件。物理系统每帧清空并重新填写它，只包含当前真正接触（有接触点）的对象。每个接触点一条 `Contact`：

| 字段 | 含义 |
| --- | --- |
| `other_body_id` | 对方刚体的标识，与对方 `Rigidbody::body_id` 对应 |
| `point` | 世界空间接触点 |
| `normal` | 从自身指向对方的法线 |
| `normal_impulse` / `tangent_impulse` | 法向 / 切向冲量 |

`Rigidbody::body_id` 由物理系统写入（尚未创建刚体时为 0），是数值而非指针，可以跨帧比较；C++ 侧可用 `CollisionResult::find(rigidbody)` 按对方刚体查找第一条接触。

#### Woolang（`je::towoo::components`）

| 函数 | 说明 |
| --- | --- |
| `Rigidbody::get_id(rb)` | 取刚体标识，尚未创建刚体时为 `none` |
| `CollisionResult::all(cr)` | 以对方标识为键的全部接触（`dict<Rigidbody::Id, collide_result>`） |
| `CollisionResult::check(cr, another)` | 是否与给定刚体接触（`option<collide_result>`） |

```woolang
UPDATE!
{
    // 与任何物体接触时，给动态刚体一个向上的速度
    system! bounce(
        vel: Physics2D::LinearVelocity,
        contacts: Physics2D::CollisionResult,
    )
    contains Physics2D::DynamicBody;
    {
        if (!contacts->all->empty)
        {
            let v = vel.value->get;
            vel.value->set((v[0], 5.0) as vec2);
        }
    }
}
```

`collide_result` 的 `position` / `normalize` 均为 `public`，脚本侧可直接读取（冲量等其余字段只在 C++ 侧可见）。

#### C++（`jeecs/components.hpp`）

```cpp
using namespace jeecs;
using namespace jeecs::slice_requirement;

struct BounceSystem : public game_system
{
    BounceSystem(game_world w) : game_system(w) {}

    void LateUpdate() // 晚于物理阶段，读到本帧结果
    {
        for (auto&& [vel, result] : query<view typesof(
            Physics2D::LinearVelocity&, Physics2D::CollisionResult&)>())
        {
            for (auto& contact : result.contacts)
                if (contact.normal.y < -0.5f) // 对方在下方：着地
                    vel.value.y = 5.f;
        }
    }
};
```

### 相关术语

参见[术语表](README.md)：

* [变换](transform.md)——物理写回的对象；
* [系统](system.md)、[世界](world.md)——物理系统的注册与执行阶段；
* [差异时间](deltatime.md)——每帧模拟推进的时长；
* [瓦片地图](tilemap.md)——`Tilemap::Wall` 自动为地图生成静态碰撞体。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。实现主体位于 `src/jeecs_core_physics_system.cpp` / `.hpp`，Box2D 为子模块 `3rd/box2d`（v3.1.1），配置宏见 `je/physics2d/config.wo`，Woolang 绑定见 `je/towoo/towoo.wo` 与 `src/jeecs_woolang_towoo_script_basic.cpp`。

### PhysicsUpdate 的五个子阶段

`Physics2DUpdatingSystem` 只实现 `PhysicsUpdate`（占用 `m_physics_update` 槽位），内部顺序为：

```
1. phase_sync_worlds        解析 Scene 配置，创建 / 复用 / 更新每层的 b2World
2. phase_sync_bodies_in     组件 → Box2D：创建刚体、同步类型/速度/位姿、按需重建形状
3. phase_step               各层并行 b2World_Step，回收本帧未出现的刚体；随后 m_worlds = this_frame
4. phase_sync_bodies_out    Box2D → 组件：写回位姿与速度
5. phase_collect_contacts   填写 CollisionResult
```

写回改动的是 `Translation` 的 `world_position` / `world_rotation`；物体矩阵 `object2world` 由变换系统在随后的 `CommitUpdate` 阶段统一重算，所以本帧渲染即可看到物理结果。

### 配置脚本的加载

`load_scene_physics_config_from_path` 用 `je_file_open` 读入脚本，`wo_load_binary` 编译，在**独立的临时 VM** 中 `woort_bootup` 执行（`woort_vm_session_guard` 负责换回原 VM 并关闭临时 VM），然后按**字段下标**解析返回值：`SceneConfig` 的 0 号字段为层列表、1 号为组列表；`LayerInfo` 依次为层号、重力、子步数、休眠、连续碰撞。改动 `config.wo` 中的结构体字段顺序必须同步修改加载器。

结果按路径缓存：路径不变不重载；加载失败时记住这个坏路径（不逐帧重试），并保留此前成功加载的配置。组数超过 `MAX_GROUP_COUNT`（16）时截断并告警。

### 层与 b2World

`m_worlds` 以层号为键持有 `PhysicsWorld`（`b2WorldId`、配置快照、组表、刚体表）。`b2CreateWorld` / `b2DestroyWorld` 会操作 Box2D 进程级的世界槽池且没有内部同步，而多个 ECS 世界可能在并行工作线程中同时启动物理，因此世界的创建与销毁由全局互斥量 `b2_world_lifecycle_mutex` 串行化；创建完成后各世界的步进与设置互相独立。

配置变化按快照做差量应用：重力改变时唤醒该层全部刚体，使新重力立即生效；休眠与连续碰撞开关变化时调用对应的 Box2D 设置；重复的层号报错并忽略后者。

⚠️ 注意 `b2DestroyWorld` 只在系统析构时调用。层从配置中消失、或 Scene 实体被移除时，`PhysicsWorld` 随 `m_worlds` 的替换被丢弃，但它持有的 b2World 没有被销毁（Box2D 世界槽上限为 `B2_MAX_WORLDS` = 128）。

### 刚体记录与形状重建

每个刚体对应一条 `BodyRecord`：实体句柄、`b2BodyId`、形状 id、形状/材质的差量缓存与 `last_seen_frame`。`phase_sync_bodies_in` 对每个实体**线性扫描**本层刚体表、以实体句柄匹配记录；找不到则按当前位姿新建 b2Body。随后无条件设置刚体类型、Bullet、固定旋转；速度、阻尼、重力缩放与位姿只在和 Box2D 当前值不一致时才写入。

⚠️ 注意 记录以完整实体句柄（chunk、槽位、版本）匹配。实体发生原型迁移（增删组件）或被碎片整理搬动后句柄会改变，系统会把它当作新实体重新创建 b2Body（`body_id` 随之改变），旧刚体在本帧步进后被回收；运动状态只能通过 `LinearVelocity` / `AngularVelocity` 组件的值延续。

形状只在碰撞体种类、几何参数、材质或触发器标记变化时重建。`Collider::Mesh` 的分解：凸且顶点数不超过 `B2_MAX_POLYGON_VERTICES`（8）时生成单个凸包多边形，否则用耳切法三角化，作为同一刚体上的复合形状；无法生成任何形状时报错。

碰撞过滤位在形状重建时计算：实体匹配的每个组 i 置 `categoryBits` 的第 i 位，`maskBits` 为所匹配各组碰撞掩码的并集。⚠️ 注意 过滤位**只在重建形状时**更新——实体的组件构成改变了它的组归属、但形状未重建时，旧的过滤位仍然生效。

### 步进与回收

`phase_step` 对各层 `parallel_foreach`，以系统的 `deltatime()` 和该层子步数（0 视为 1）调用 `b2World_Step`——**没有固定步长累加器**，模拟步长随帧时间变化。步进后扫描刚体表，`last_seen_frame` 不是本帧的记录（实体已销毁或不再满足切片）随即 `b2DestroyBody`。

### 写回与接触收集

`phase_sync_bodies_out` 的切片**强制要求** `LocalPosition` 与 `LocalRotation`，借助 `Translation::set_global_position` / `set_global_rotation` 反算局部量；偏移在这里被扣除，锁定轴保留原世界坐标，锁定轴的速度写为 0。

`phase_collect_contacts` 用 `b2Body_GetContactCapacity` / `b2Body_GetContactData` 读取接触（复用 `m_contact_data_scratch` 避免逐帧分配），跳过没有接触点的接触，把法线翻转为“自身指向对方”，每个流形点输出一条 `Contact`；`is_trigger` 取自对方记录缓存的触发器标记。

⚠️ 注意 Box2D v3.1 的宽相阶段会直接跳过含传感器形状的形状对（`broad_phase.c`：“Sensors are handled elsewhere”），传感器重叠只通过传感器事件报告，而本系统没有读取传感器事件。因此触发器之间、触发器与普通形状之间的重叠**不会出现在 CollisionResult 中**，`is_trigger` 实际上不会为 true。

Woolang 绑定中的 `Rigidbody::Id` 是把 `body_id` 数值伪装成 handle 的不透明值。`CollisionResult::all` 以它为键构造字典——同一对方的多个接触点会互相覆盖，只保留最后一条；`check` 走 `CollisionResult::find`，返回第一条匹配。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 物理组件与字段注释 | `include/jeecs/components.hpp`（`namespace Physics2D`） |
| 物理系统与五个子阶段 | `src/jeecs_core_physics_system.cpp` / `.hpp` |
| 配置脚本宏与 SceneConfig 结构 | `je/physics2d/config.wo` |
| 配置脚本模板 | `builtin/editor/Editor/Template/physics.je4physics.wo.template` |
| Woolang 绑定 | `je/towoo/towoo.wo`、`src/jeecs_woolang_towoo_script_basic.cpp` |
| 编辑器组件预设 | `builtin/editor/Editor/ComponentPresets.wo` |
| Box2D 源码 | `3rd/box2d`（v3.1.1） |
