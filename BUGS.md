# 引擎已知问题清单

本清单来自 2026-11 概念文档（`doc/concept/`）撰写过程中对源码的逐项核对，每条问题在对应概念文档的 ⚠️ 段落也有记载。2026-11-10 已完成一轮排查核实与修复，各条状态见每项的 **状态** 行。

标记说明：

* **[已复现]**——已用最小用例 / 编译检查 / 数值计算验证过触发；
* **[源码核对]**——由阅读实现代码得出，未做运行时验证。

状态标记：

* **已修复**——本次修复并已提交；
* **不修复**——经核实属刻意设计或语言/平台限制；
* **跳过**——按决策本轮不处理。

## 脚本与 ECS

### BUG-01 `system!` 的 `except` / `anyof` 过滤条件效果相反 [源码核对]

* **状态：已修复**（Woolang 侧枚举顺序对齐 C 侧 `CONTAINS/MAYNOT/EXCEPT/ANYOF`）
* 位置：`je/towoo/system.wo`（`require_type` 枚举）与 `src/jeecs_woolang_towoo_script_basic.cpp`（`wojeapi_towoo_register_system_job` 按整数强转）
* 原因：Woolang 侧枚举仍是旧顺序 `CONTAINS=0, MAYNOT=1, ANYOF=2, EXCEPT=3`；C 侧枚举在 af439c9f 之后为 `EXCEPT=2, ANYOF_0=3`
* 触发流程：
  1. 在系统声明中写 `except Transform::LocalToParent;`
  2. 注册系统并运行
  3. 实际行为：切片变为**必须包含** `LocalToParent`（与意图相反）；同理 `anyof A, B;` 会把 A、B **全部排除**
* 影响范围：仅 Woolang `system!` 声明；C++ 切片与 `entity_selector` 不受影响
* 参考：`doc/concept/slice.md`

### BUG-02 `component!` 的 `woolang` 值成员不可用 [已复现]

* **状态：已修复**（补回 `static`）
* 位置：`PinValue::JEScriptTypeName / JEScriptTypeDeclare`（commit 42c8bbf0 拆分 PinValue 时丢失 `static`）
* 触发流程：
  1. 在 `component!` 中声明值成员，如 `count: woolang int32 = 0;`
  2. `has_JEScriptTypeInterface` 概念检测恒为 false（GCC 16 与 MSVC 2022 均实测）
  3. 生成的 `types.wo` 不再包含 `dynamic` 声明，该成员无法使用
* 参考：`doc/concept/component.md`

### BUG-03 自动生成的 `components.wo` 与运行时成员错位 [源码核对]

* **状态：已修复**（`create_component_struct` 跳过无解析器成员，槽位与生成器一致）
* 位置：`types.wo` 生成逻辑（跳过没有脚本解析器的成员类型）× 运行时结构体构造（按全部成员填充）
* 触发流程：
  1. 声明一个 C++ 组件，成员列表中含一个**没有脚本解析器**的类型，且其后还有其它成员
  2. 脚本侧访问其后成员时，槽位下标按"跳过后的布局"计算，而实际数据按"全量布局"存放
  3. 结果：读到错误成员的数据
* 参考：`doc/concept/component.md`

### BUG-04 Woolang `je::smooth_deltatime()` 返回实时差异时间 [源码核对]

* **状态：已修复**（改为调用 `smooth_deltatimed()`）
* 位置：`wojeapi_smooth_deltatime`（内部误调 `deltatimed()`）
* 触发流程：脚本中调用 `je::smooth_deltatime()` 并与 `je::real_deltatime()` 对比 → 两者返回相同值，平滑加权（9:1）从未生效
* 参考：`doc/concept/deltatime.md`

### BUG-05 注销 pre for-worlds 工作时调用未初始化函数指针（UB）[源码核对]

* **状态：已修复**（unregister 路径的 `universe_action` 改为值初始化 `{}`）
* 位置：`src/jeecs_basic_arch_system.cpp`，`REMOVE_PRE_JOB_FOR_WORLDS` 分支
* 触发流程：
  1. 调用 `je_ecs_universe_pre_for_worlds_job_register` 注册一个 pre for-worlds 工作
  2. 调用对应的 `je_ecs_universe_pre_for_worlds_job_unregister` 注销
  3. 分支内调用 remove action 自身的 `m_free_function`，该字段从未被初始化 → 未定义行为
* 参考：`doc/concept/universe.md`

### BUG-06 `je::file::cache::readall` 读出内容开头多 8 字节 [源码核对]

* **状态：已修复**（忽略校验路径同样跳过 8 字节校验字段）
* 位置：缓存读取路径；以忽略校验（`virtual_crc64 = -1`）方式打开时，校验字段未被跳过，读取位置停在偏移 8
* 触发流程：
  1. 以 `-1` 校验模式打开 `je::file::cache`
  2. 调用 `readall` → 返回内容开头多出 8 字节
* 影响：编辑器“最近打开项目”记录使用的就是该接口，修复前写入的记录可能已损坏
* 参考：`doc/concept/cache.md`

### BUG-07 跨帧持有的实体句柄是悬垂指针（UB）[源码核对]

* **状态：已修复**（搬空的 chunk 回收到 ArchType 级复用池而非 `delete`，旧句柄靠版本号失配安全失效；`game.hpp` 注释已补机制说明）
* 位置：碎片整理会直接 `delete` 搬空的 chunk（`src/jeecs_basic_arch_system.cpp`）
* 触发流程：
  1. 在帧 N 保存一个普通实体的句柄（非预制体）
  2. 该实体随后被销毁，且所在 chunk 被整理搬空并 `delete`
  3. 帧 N+1 使用旧句柄 → 解引用已释放内存
* 规避：跨帧引用仍建议使用 `Anchor` / `LocalToParent` 的 uuid，或实体名
* 参考：`doc/concept/entity.md`

## 数学

### BUG-08 Woolang `ivec2` 不支持乘除运算 [已复现]

* **状态：已修复**（`: ivec2` 类型标注改为 `as ivec2` 类型转换）
* 位置：`je/towoo/towoo.wo`，`ivec2` 的乘、除运算符把结果写成 `: ivec2`（类型标注）而非 `as ivec2`（类型转换）
* 触发流程：脚本中写 `let v = ivec2(2, 4) * 2;` 或 `/ 2` → 编译失败；加减正常
* 参考：`doc/concept/math.md`

### BUG-09 Woolang 标量在左的数乘编译失败 [已复现]

* **状态：不修复（语言设计限制）**——Woolang 运算符只解析左操作数类型提供的重载（左操作数恒为首个参数），`2. * v` 需要为基础类型 `real` 重载运算符，而开发规范禁止为基础类型重载运算符。脚本侧请写 `v * 2.`（`je/towoo/towoo.wo` 已留注释说明）。
* 位置：Woolang 数乘运算符定义
* 触发流程：脚本中写 `2. * v` → 编译失败；只能写 `v * 2.`
* 参考：`doc/concept/math.md`

### BUG-10 `quat::rotation(a, b)` 旋转方向与名字相反 [已复现]

* **状态：已修复**（轴改为 `a × b`，C++ 与 Woolang 两侧统一；无既有调用点，改向安全）
* 位置：`include/jeecs/math.hpp`
* 触发流程：调用 `quat::rotation(a, b)`，期望得到把 `a` 转到 `b` 的旋转 → 实际得到的是把 `b` 转到 `a` 的旋转（已用数值计算验证）
* 参考：`doc/concept/math.md`

### BUG-11 C++ 与 Woolang 的四元数插值行为不一致 [源码核对]

* **状态：已修复**（Woolang 侧 `lerp`/`axis` 结果单位化，与 C++ 构造函数行为一致）
* 位置：C++ `quat::lerp` / `axis_angle` 会自动单位化；Woolang 侧 `lerp` / `axis` 不会
* 触发流程：对非单位四元数（或多次插值累积后偏离单位长度）做插值 → 两侧结果不同，Woolang 侧结果随迭代愈发偏离单位四元数
* 参考：`doc/concept/math.md`

### BUG-12 `math::random` 线程不安全 [源码核对]

* **状态：已修复**（`mt19937` 改为 `thread_local`）
* 位置：函数内静态 `mt19937`，无锁
* 触发流程：
  1. 引擎内置粒子系统（并行阶段）与用户系统的更新函数同帧并行执行
  2. 两边同时调用 `math::random()` → 对同一 `mt19937` 的数据竞争
* 参考：`doc/concept/math.md`

## 图形 API 与资源

### BUG-13 `shader::set_uniform` 设置 int 值直接 panic [已复现]

* **状态：已修复**（补 `else if`，与 `bound_shader` 版本一致）
* 位置：`je/graphic.wo`，`shader::set_uniform` 在 `if (val is int)` 分支后缺少 `else`，写入 int 后落入 `std::panic`
* 触发流程：
  1. 编辑器材质面板给 int 型 uniform 赋值（`builtin/editor/Editor/Material.wo:275`）
  2. 或场景保存/恢复路径（`builtin/editor/Editor/Saving/Entity.wo:687`）
  3. → 脚本 panic（最小复现已验证）
* 参考：`doc/concept/shader.md`

### BUG-14 `texture2d::clip` 把裁剪宽度当作源纹理行跨度 [源码核对]

* **状态：已修复**（源侧行跨度按源纹理宽度计算，裁剪矩形与源边界求交）
* 位置：`include/jeecs/graphic.hpp` 的 `texture2d::clip`
* 触发流程：调用 `clip(src, x, y, w, h)` 且 `w < 源纹理宽度` → 按错误的行跨度读取，结果错乱；且无越界检查。仅当 `w ≥ 源宽度` 时结果正确
* 参考：`doc/concept/texture.md`

### BUG-15 `ray::intersect_mesh` 不读索引缓冲 [源码核对]

* **状态：已修复**（带索引网格按 `m_indices` 取顶点；无索引时上界用顶点数 `m_vertex_length / m_data_size_per_point`）
* 位置：`include/jeecs/ray.hpp`
* 触发流程：
  1. 对**带索引**的网格（模型文件加载产物）调用射线相交测试
  2. 实现按顶点数组顺序遍历，循环上界却使用 `m_index_count` → 结果不可靠，且可能越界读取
* 影响：编辑器实体拾取（`EntityPicking.wo`，`consider_mesh=true`）走的就是这条路径
* 参考：`doc/concept/ray.md`

### BUG-16 `vertex::create` 传空 `formats` 时除零 [源码核对]

* **状态：已修复**（先检查每顶点字节数为 0 再做除法）
* 位置：`_jegl_create_vertex_impl`，先做除法、后检查每顶点字节数是否为 0
* 触发流程：Woolang 侧 `vertex::create` 传入空 `formats` 数组 → 除零，而不是返回 `none`
* 参考：`doc/concept/mesh.md`

### BUG-17 模型加载不应用节点变换 [源码核对]

* **状态：已修复**（节点遍历累计 `mTransformation` 链并烘焙进顶点；法线取线性部分后单位化）
* 位置：Assimp 加载路径（`je_graphic_vertex_load`）
* 触发流程：加载内部子节点带变换的模型文件（所有子网格合并为一个网格时未应用节点变换）→ 子网格位置错误
* 参考：`doc/concept/mesh.md`

## 摄像机与渲染

### BUG-18 `Projection` 拷贝丢失 `znear` / `zfar` [源码核对]

* **状态：已修复**（拷贝持久配置；uniform buffer 不共享、各自新建）
* 位置：`Camera::Projection` 的拷贝构造是空函数
* 触发流程：
  1. 创建带 `Camera::Projection` 的预制体
  2. `world::instance` 实例化
  3. → 新摄像机的 `znear` / `zfar` 回到默认值 0.3 / 1000
* 参考：`doc/concept/camera.md`

### BUG-19 摄像机 `Clear` 不受 `Viewport` 限制，分屏互相清屏 [源码核对]

* **状态：跳过（暂不修复）**——彻底修复需所有图形后端支持矩形清除（GL 可用 scissor、Vulkan 有 clear rect，但 DX11/Metal 的清除 API 不支持矩形，需 quad-draw 方案）；**保证各图形后端行为一致优先**，保持现状。
* 位置：GL、DX11、Vulkan 三个后端均已确认同一行为
* 触发流程：
  1. 两个摄像机分屏（不同 `Viewport`），各自挂 `Camera::Clear`
  2. 每个摄像机清的都是**整个颜色附件 0**
  3. → 后渲染的摄像机把先渲染摄像机的画面清掉
* 附注：深度每次无条件清为 1.0；帧缓冲不会被自动清除
* 参考：`doc/concept/camera.md`

### BUG-20 挂 `CameraPostPass` 但无 `Shaders` 的摄像机会空指针解引用 [源码核对]

* **状态：已修复**——空指针解引用已在此前重构（e2c3e604）中加入的 `shaders != nullptr` 守卫消除；本次补 logwarn，使"挂 `Light2D::CameraPostPass` 但未挂 `Renderer::Shaders` → G-buffer 照常渲染却无合成、画面空白"的配置错误可见。
* 位置：DeferLight2D 合成 pass
* 触发流程：
  1. 世界中启用 DeferLight2D 管线
  2. 摄像机挂 `Light2D::CameraPostPass` 但未挂 `Renderer::Shaders`
  3. → 无 `Shaders` 时表现为屏幕无内容
* 参考：`doc/concept/light.md`、`doc/concept/camera.md`

### BUG-21 `ShadowBuffer` 尺寸跟随窗口而非渲染目标 [源码核对]

* **状态：不修复（刻意保留）**——为避免频繁申请/释放 framebuf 而保持现状；正确修法需要一套类似其他引擎的 framebuf 池化机制（按申请的缓冲区尺寸与配置自动分配、用完回收、闲置若干帧后释放），待后续作为独立任务实现。
* 位置：Light2D 阴影缓冲分配；每个光源只有一块 `ShadowBuffer`
* 触发流程：摄像机渲染到非窗口尺寸的帧缓冲（如小尺寸离屏视口）→ 阴影缓冲仍按窗口尺寸分配，与渲染目标分辨率不匹配
* 参考：`doc/concept/light.md`

### BUG-22 编辑器的“正交”判定与渲染系统不一致 [源码核对]

* **状态：已修复**（编辑器 5 处统一为“挂 `OrthoProjection` 即正交”，与渲染系统一致；见 DIFF-12）
* 位置：渲染系统按“挂了 `OrthoProjection` 就按正交处理”；编辑器按“没有透视组件才算正交”
* 触发流程：实体同时挂 `OrthoProjection` 与 `PerspectiveProjection` → 渲染按正交执行，编辑器按透视处理，两侧表现不一致
* 参考：`doc/concept/camera.md`

## 粒子与动画

### BUG-23 `local` 模拟空间下发射方向被旋转两次 [源码核对]

* **状态：不修复实现，仅修正注释**（按决策保留既有行为；`Emitter` 与粒子系统头注释已按实际行为改写：`m_direction` 始终先按发射器朝向旋转一次，局部空间下渲染时随实体变换再应用一次）
* 位置：粒子系统发射逻辑
* 触发流程：
  1. `Emitter` 的 `m_simulation_space` 设为 `local`
  2. 发射器实体带旋转
  3. → 发射方向被旋转两次
* 参考：`doc/concept/particle.md`

### BUG-24 粒子池以 chunk 内槽位号为键，跨 chunk 冲突 [源码核对]

* **状态：已修复**（键改为 chunk 指针 + 槽位号；不同 chunk 对象地址差 ≥ `sizeof(arch_chunk)` > 槽位上限，相加不重叠）
* 位置：粒子系统粒子池索引，键为 `e._m_raw._m_id`（不含 chunk 信息）
* 触发流程：
  1. 同一世界创建两个发射器实体，分处不同 chunk 且槽位号相同
  2. → 两个发射器共用同一个粒子池，粒子数据互相覆盖
* 参考：`doc/concept/particle.md`

### BUG-25 发射逻辑无条件读取可选的 `Translation` [源码核对]

* **状态：已修复**（无 `Translation` 时按 `m_direction` 原值解释）
* 位置：粒子系统切片查询中 `Translation` 为 MAYNOT（可选），发射逻辑却不判空直接读取
* 触发流程：创建不带 `Translation` 组件的发射器实体 → 发射路径解引用空指针
* 参考：`doc/concept/particle.md`

### BUG-26 动画事件两处丢失 [源码核对]

* **状态：已修复**（`_fire_events_crossed` 先按大小归一 low/high（反向推进区间不再为空）；正向区间 `low==0` 时低端改闭（0 时刻事件可触发））
* 位置：动画系统事件判定
* 触发流程一：在动作时刻 **0** 处放置事件 → 正向播放永不触发（判定区间为 `(上次时刻, 本次时刻]`，左开）
* 触发流程二：反向推进（`pingpong` 回程或负 `speed`）→ 最后一段把 `(from, to)` 按原顺序传入，区间为空，回程中途的事件全部丢失
* 参考：`doc/concept/animation.md`

## 物理与音频

### BUG-27 `CollisionResult` 的 `position` / `normalize` 未标 `public`，Woolang 无法读取 [已复现]

* **状态：已修复**（`collide_result` 结构成员补 `public`；Woolang 结构成员默认私有）
* 位置：`Physics2D::CollisionResult::Contact` 成员声明（Woolang 侧 `collide_result`）
* 触发流程：脚本中 `get_component(CollisionResult)` 后访问 `.position` / `.normalize` → `baozi check` 编译失败；此前脚本侧只能判断是否接触
* 参考：`doc/concept/physics.md`

## 引擎与模块

### BUG-28 静态链接的模块启动时不执行入口 [源码核对]

* **状态：跳过（按决策本轮不处理）**。核实结论留档：模块源码写 `jeecs_module_entry`/`jeecs_module_leave`，静态构建时由模块模板 CMakeLists 以 `-Djeecs_module_entry=jestatic_module_<名>_entry` 宏重命名（见 `build/builtin/editor/Editor/Template/CMakeLists.txt.template`）；prebuild 生成的伪库表（`script/prebuild/prebuild.wo`）只登记 `jeecs_module_entry` 符号，而 woort（`3rd/woort/src/woort_dylib.c` 的 `_woort_dylib_registry_insert`）注册伪库时只自动调用 `woort_lib_entry`（静态包正因此自动执行）。静态模块入口仍需脚本侧 `je::load_module` 手动触发。
* 位置：prebuild 为模块生成的伪造库登记的是 `jeecs_module_entry`，而 woort 加载库时只调用 `woort_lib_entry`
* 触发流程：
  1. 以 `JE4_STATIC_LINK_MODULE_AND_PKGS=ON` 构建含 `module/*.je4module` 的引擎
  2. 启动引擎 → 模块入口不会自动执行
  3. 需在脚本中以模块名调用 `je::load_module` 手动触发
* 参考：`doc/concept/module.md`

### BUG-29 `graphic_syncer` 未初始化 [源码核对]

* **状态：已修复**（成员声明处补 `= nullptr`）
* 位置：`include/jeecs/engine.hpp`，`game_engine_context` 的成员无初值
* 触发流程：构造 `game_engine_context` 后不调用 `prepare_graphic` 直接析构 → 读到未初始化的指针
* 参考：`doc/concept/entry.md`

### BUG-30 宇宙停止后调用 `wait()` 死锁 [源码核对]

* **状态：已修复**（退出回调的注册与排水加同一把锁并置退出标记；线程已退出时注册失败，`je_ecs_universe_loop` 直接返回）
* 位置：`game_universe::wait()` / `je_ecs_universe_loop`
* 触发流程：
  1. 宇宙生命周期归零、更新线程退出
  2. 之后调用 `wait()` → 永久阻塞
* 参考：`doc/concept/universe.md`

### BUG-31 `Translation::set_global_rotation` 在 3D 下计算错误 [源码核对]

* **状态：已修复**（改为左乘 `parent⁻¹ * r`，与 `set_global_position` 同一模式）
* 位置：`include/jeecs/components.hpp`，计算 `r × parent⁻¹`，正确应为 `parent⁻¹ × r`
* 触发流程：
  1. 3D 场景中父实体带旋转
  2. 对子实体调用 `set_global_rotation`
  3. → 局部旋转计算错误（仅当旋转可交换——如纯 2D 绕 Z 轴——时恰好等价，不显现）
* 参考：`doc/concept/transform.md`
