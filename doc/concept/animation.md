# 动画（Animation）

本文档是[术语表](README.md)中「动画」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述动画的概念模型、播放控制与事件；
* **第二部分·底层实现**——面向引擎维护者，描述上述行为在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 动画是什么

动画（Animation）随时间改变实体的组件数据（或着色器 uniform），借此影响渲染结果。JoyEngine 的动画采用*轨道 + 关键帧*模型（v2）：动画资源描述“哪个组件的哪个成员，在哪个时刻取什么值”；实体上的 `Animation::Animator` 组件负责播放；`Animation::AnimatorSystem` 每帧插值出当前值并写回实体。

### 概念模型

```
动画集（.je4animation 文件）
 └── 动作（action）：名字、循环方式、时长
      ├── 轨道（track）× N：作用于“组件成员”或“着色器 uniform”
      │    └── 关键帧（key）× M：时刻、值、到下一关键帧的插值方式
      └── 事件点（event）× K：时刻、名字
```

* 一个*动作*里的所有轨道与事件共享同一条时间轴；动作时长可省略，缺省为全部关键帧与事件时刻的最大值；
* *轨道*的目标二选一：**组件成员**（如 `Transform::LocalPosition` 的 `pos`）或**着色器 uniform**（经实体的 `Renderer::Shaders` 写入）；
* 值类型：`int` / `float` / `vec2` / `vec3` / `vec4` / `quat4`。成员轨道的值类型必须与成员类型严格一致（`vec4` 也可作用于颜色成员），不匹配的轨道在加载时被丢弃并记录错误；`quat4` 在源文件中以欧拉角（度）书写；
* 轨道模式：`absolute`（直接赋值，默认）或 `additive`（在原值上叠加，四元数为相乘）。

### 插值、切线与循环

| 概念 | 取值 | 说明 |
| --- | --- | --- |
| 插值（本关键帧到下一个） | `constant` | 保持当前值直到下一关键帧（阶跃，适合精灵图逐帧切换） |
| | `linear`（默认） | 线性插值；`quat4` 为球面插值，`int` 线性后取整 |
| | `hermite` | 三次曲线，使用关键帧切线；只对 `float` / `vec*` 开放 |
| 切线（hermite 用） | `auto`（默认）/ `linear` / `free` | Catmull-Rom 自动切线（端点、局部极值与平台处取 0，避免过冲）/ 与相邻关键帧连线的斜率 / 源文件给出的 `tangent_in`、`tangent_out` |
| 循环 | `none`（默认）/ `loop` / `pingpong` | 播到结尾停住并结束 / 回绕到开头继续 / 到达端点后反向播放 |

时刻落在首个关键帧之前或最后一个关键帧之后时，取端点值。

### Animator 组件

| 成员 | 说明 |
| --- | --- |
| `animation` | 动画绑定：动画集路径 + 可选的 *autoplay* 动作（脚本中名为 `m_path` / `m_action`） |
| `speed` | 播放速度倍率，默认 1 |

其余成员（当前动作、播放时间、过渡状态、本帧事件等）是运行时状态，不参与序列化。行为约定：

* 绑定路径变化后，系统重新加载动画集、**停止**当前播放并重置 autoplay 记录；加载失败时以空动画占位，组件始终可用；
* autoplay 动作在每个取值上只触发一次（换源后可再次触发）；清空 autoplay 只重置记录，不打断当前播放；
* 世界中必须注册 `Animation::AnimatorSystem`。编辑器新建世界时把它登记为“运行时系统”，只在调试运行与导出时加入世界，编辑状态下动画不会播放。

### 播放控制

Woolang 的 `je::animation` 与 C++ 的 `jeecs::animation`（头文件 `jeecs/animation.hpp`）提供同一组函数，参数都以实体开头：

| 函数 | 说明 |
| --- | --- |
| `play(e, name)` | 立即从头播放指定动作，清除过渡 |
| `crossfade(e, name, seconds)` | 在给定时长内从当前动作平滑过渡到指定动作 |
| `stop(e)` | 清除动作与进度；组件保持最后一次写入的值，不会复原 |
| `pause(e)` / `resume(e)` | 暂停（每帧仍写回当前姿态）/ 恢复 |
| `is_playing(e)` | 已指定动作、未暂停且未结束（非循环动作播到结尾即为结束） |
| `current_action(e)` | 当前动作名，未播放时为空（Woolang 为 `option::none`，C++ 为空指针） |
| `duration(e)` / `progress(e)` | 当前动作总时长（秒）与进度（0 ~ 1），未播放时为 0 |
| `events(e)` | 本帧触发的事件名（仅 Woolang；C++ 可直接读 `Animator::m_frame_events`） |

这些调用直接修改 Animator 的播放状态，效果在下一次动画系统更新时体现。

⚠️ 注意 播放控制函数要求实体持有 `Animation::Animator`，否则触发断言。

### 动作过渡（crossfade）

过渡期间新旧两个动作同时推进：旧动作以权重 1 − w、新动作以权重 w 采样后混合，w 从 0 线性增长到 1，到 1 时新动作接替为当前动作。以下情况 `crossfade` 等价于 `play`：当前没有动作、当前动作已结束、时长 ≤ 0、目标就是当前动作；对正在过渡的同一目标重复调用会被忽略。过渡时长按 `speed` 缩放后的时间计算。

### 动画事件

播放越过事件点时触发事件，读取 `events(e)` 即可得到本帧触发的事件名：

* `loop` 回绕时越过的事件同样会触发；过渡期间两个动作的事件都会触发；
* 事件在每帧的状态更新阶段清空并重新生成，因此 `UPDATE!` / `LATE_UPDATE!` 读到的是本帧事件，`PRE_UPDATE!` 读到的是上一帧的；
* 读取不消费事件，同一帧内多个系统都能读到相同结果；
* 每个实体每帧最多记录 32 个事件，超出部分丢弃并告警。

### 示例

#### Woolang（`je::animation`）

```woolang
import je::animation;   // 其余导入同 towoo 系统模板

UPDATE!
{
    system! hero_anim()
        contains Animation::Animator;
    {
        if (!je::animation::is_playing(e))
            je::animation::crossfade(e, "idle", 0.2);

        for (let ev : je::animation::events(e))
            if (ev == "footstep")
                je::log("脚步声");
    }
}
```

#### C++（`jeecs::animation`）

```cpp
void HeroSystem::Update()
{
    for (auto&& [e, animator] : query_entity<view typesof(
        Animation::Animator&)>())
    {
        if (!animation::is_playing(e))
            animation::crossfade(e, "idle", 0.2f);

        for (auto& ev : animator.m_frame_events)
            if (ev == "footstep")
                animation::play(e, "jump");
    }
}
```

### 动画文件与缓存

动画集以 TOML 文本保存为 `.je4animation`，通常由编辑器的动画编辑器（关键帧与曲线双时间轴、精灵图切片、预览播放）写出，也可以手写：

```toml
version = 2

[[actions]]
name = "walk"
loop = "loop"
duration = 0.4            # 省略时取 0.2（最后一个关键帧/事件的时刻）

[[actions.events]]
time = 0.2
name = "footstep"

[[actions.tracks]]
component = "Renderer::Textures2D"
member = "offset"
type = "vec2"
[[actions.tracks.keys]]
time = 0.0
value = [0.0, 0.0]
interp = "constant"
[[actions.tracks.keys]]
time = 0.2
value = [1.0, 0.0]
```

运行时并不直接读 TOML，而是读取同目录的[缓存](cache.md) `<路径>.je4cache`：缓存缺失、格式版本变化或源文件内容变化时自动重新生成。导出项目时编辑器会预先为全部 `.je4animation` 生成缓存。

v2 不兼容也不迁移旧版本：缓存生成器与动画编辑器都只接受 `version = 2`，其它版本的文件会报错，并提示在动画编辑器中重新创建。

### 相关术语

参见[术语表](README.md)：

* [组件](component.md)——动画写入的目标，成员需要注册反射才能被轨道定位；
* [系统](system.md)、[帧](frame.md)——动画系统在每帧的状态更新阶段推进；
* [着色器](shader.md)——uniform 轨道写入实体的着色器；
* [缓存](cache.md)——动画资源以 `.je4cache` 形式被运行时读取；
* [编辑器](editor.md)——动画编辑器负责创作 `.je4animation`。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。播放与采样位于 `src/jeecs_core_animation_system.cpp/.hpp`，资源加载位于 `src/jeecs_graphic_animation_cache.cpp`，缓存生成器为 `builtin/script/internal/cache/animation.wo`，C ABI 类型与各接口注释见 `include/jeecs/animation.hpp`，组件定义见 `include/jeecs/components.hpp` 的 `Animation::Animator`。

### 系统在帧中的位置

`AnimatorSystem` 只实现 `StateUpdate()`，即世界内八个更新阶段中的第 2 个（`m_state_update`）：在脚本的 PRE_UPDATE 之后、UPDATE 之前。因此 UPDATE 中的逻辑看到的是本帧已动画化的组件值，也可以覆盖它们；变换阶段（第 4 个）在其后，被动画改动的局部变换当帧就会结算到 `Translation`。

每个 `Animator&`（+ 可选 `Renderer::Shaders*`）实体依次执行：清空 `m_frame_events` → 路径变化则重新加载 → 处理 autoplay → `_update_animator`。

### `_update_animator`

1. 无动作则返回——这就是 `stop` 之后组件保持原值的原因；
2. 按名查找当前动作，找不到则告警并停止；
3. 暂停时以权重 1 采样 `m_time` 并写回，然后返回；
4. `scaled_dt = 系统差异时间 × speed`，`_advance_time` 按循环方式推进时间、处理端点并触发越过的事件；
5. 存在过渡目标时同样推进它，`m_fade_elapsed += |scaled_dt|`，`w = elapsed / duration`，w ≥ 1 时 `finish_fade` 让目标接替；目标动作丢失则放弃过渡；
6. 当前动作以 1 − w、目标动作以 w 采样进栈上的采样缓冲，再一次性写回实体。

`_fire_events_crossed` 的区间约定是：正向 (low, high]、反向 [low, high)，且正向区间起点为 0（播放起点、循环回到起点）时低端为闭。函数内部先按大小归一调用方传入的端点（反向推进时 from > to），各分支统一以 `(from, to, io_direction)` 传入也不会出现空区间。

### 采样与插值

`_sample_track` 二分定位区段 `keys[i] ≤ t < keys[i+1]`，越界钳制到端点；CONSTANT 返回前一关键帧，LINEAR 走 `_mix_values`（QUAT4 slerp、INT 取整），HERMITE 对每个通道用 `_hermite(p0, p1, m0, m1, dt, u)`，m0 取前一关键帧的出导数、m1 取后一关键帧的入导数（切线布局为每通道“入, 出”）。

曲线数学的唯一来源是 Woolang 的 `je::animation::curve`（`auto_tangent` / `linear_tangents` / `hermite`）：缓存生成器用它把 AUTO / LINEAR 切线烘焙成导数值写入缓存，动画编辑器的预览也复用它，保证所见即所得；C++ 侧的 `_hermite` 是同一公式，运行时不再计算切线。

### 采样缓冲

`je_animation_sample_buffer_t` 按（轨道种类, 是否叠加, 组件类型 + 成员 / uniform 名）合并条目：赋值条目做加权平均——新值以 w / (W + w) 混入已累计的值（QUAT4 为 slerp），W 随之累加；叠加条目累加 value × weight。权重只在同一目标的多个条目之间起作用：只出现在其中一个动作里的轨道没有混合对象，会按其采样值原样写入。

写回时成员轨道按“组件地址 + 成员偏移”直接写内存（实体缺该组件则跳过，`math::color` 成员单独处理），uniform 轨道对实体 `Renderer::Shaders` 的全部 pass 调用 `set_uniform`（不支持 QUAT4）。C API 把同一套缓冲暴露给需要自定义混合的调用方：`create` / `clear` / `action_sample`（权重 ≤ 0 忽略，时刻钳制、不回绕）/ `apply` / `destroy`。

⚠️ 注意 uniform 轨道修改的是**共享的着色器资源**，会影响所有使用该着色器实例的实体。

### 加载与缓存

`graphic::animation::load(path)` → `je_animation_open`：空路径返回不含动作的空动画；否则经 `je_animation_cache_open_or_create` 取得缓存文件。缓存以格式版本（`ANIMATION_CACHE_VERSION`）与源文件内容的 CRC64 校验，未通过时在新建的 VM 中调用生成器脚本的 `generate(cache, source)` 重写；生成成功、`je_cache_file_close(..., true)` 回写主版本号后缓存才生效。

加载器解析 `je_cache_binary_*` 文档：按名字查找组件类型与成员、校验值类型与成员类型，无效条目逐条记录日志并跳过；关键帧按时间稳定排序；结果是不含 C++ 容器、只读的 `je_animation_t` 连续数组，由 `je_animation_close` 释放。加载失败时 `graphic::animation::load` 回退为空动画，因此 `Animator::m_animation_resource` 永远有效。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| C ABI 类型、接口注释、C++ 播放封装 | `include/jeecs/animation.hpp`（逐项说明见 `doc/capi/animation/`） |
| Animator 组件与 playback_state | `include/jeecs/components.hpp`（`Animation::Animator`） |
| 动画系统、采样缓冲、播放控制实现 | `src/jeecs_core_animation_system.cpp/.hpp` |
| 缓存读取；缓存打开/生成入口 | `src/jeecs_graphic_animation_cache.cpp`；`src/jeecs_basic_internal_scripts.cpp` |
| 缓存生成器（TOML → 二进制） | `builtin/script/internal/cache/animation.wo` |
| Woolang 封装与曲线数学 | `je/animation.wo`（`je::animation`、`je::animation::curve`） |
| 动画编辑器 | `builtin/editor/Editor/Animation/` |
