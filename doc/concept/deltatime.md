# 差异时间（DeltaTime）

本文档是[术语表](README.md)中「差异时间」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述差异时间的含义、实时与平滑两种取值、时间缩放与最大差异时间，以及获取方式；
* **第二部分·底层实现**——面向引擎维护者，描述差异时间的计算公式、初值，以及时间缩放的生效位置。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 差异时间是什么

差异时间（DeltaTime，常简写为 dt）是**上一帧开始到本帧开始所经过的时间**，单位为秒。它由宇宙在每帧开始时计算一次（见[一帧的四个步骤](frame.md#一帧的四个步骤)），同一宇宙中的所有世界、所有系统读到的是同一个值。

### 为什么需要差异时间

一帧的实际耗时并不固定：显示器刷新率不同、负载起伏、偶发卡顿，都会让帧与帧之间的间隔忽长忽短。如果逻辑写成“每帧移动 0.1”，物体的速度就会随帧率变化；把变化量写成**速率 × 差异时间**，结果就只取决于真实经过的时间，与帧率无关：

```woolang
UPDATE!
{
    system! move(localpos: Transform::LocalPosition)
    {
        // 每秒沿 x 轴移动 2 个单位，与帧率无关
        let p = localpos.pos->get;
        localpos.pos->set(p + (2. * je::deltatime(), 0., 0.) as vec3);
    }
}
```

### 实时与平滑

| 取值 | 计算方式 | 特点 |
| --- | --- | --- |
| *实时差异时间*（real） | 本帧开始时刻减上一帧开始时刻，并以最大差异时间截断 | 如实反映上一帧的耗时，帧间抖动原样体现 |
| *平滑差异时间*（smooth） | 历史实时差异时间的加权平均，每帧新值占 1/10 的权重 | 变化平缓，对偶发的长帧、短帧不敏感，但对帧率的真实变化反应滞后 |

选用建议：

* **默认使用实时差异时间**：计时器、冷却、动画推进、响应输入的位移等需要与真实时间对齐的逻辑——只有实时值的累加才等于真实经过的时间；
* 平滑差异时间适合对**观感**敏感、对精确累计不敏感的场合，例如相机跟随的插值速度、帧率显示，它能削弱单帧抖动造成的顿挫；
* 不要用平滑差异时间做计时，它的累加与真实时间并不相等。

### 时间缩放

*时间缩放*（timescale）是宇宙级的系数，**引擎返回的实时与平滑差异时间都已乘上它**：

* 1.0（默认）为正常速度，0.5 为半速慢动作，2.0 为两倍速；
* **0 是合法值**：读到的差异时间恒为 0，所有由差异时间驱动的逻辑随之停止，可以用来实现暂停；负数会被当作 0；
* 时间缩放只缩放读到的数值，**不改变帧率**：系统照常每帧执行，不依赖差异时间的逻辑（例如按帧计数）不受影响；
* 引擎内置的物理、动画、粒子等系统同样读取缩放后的差异时间，因此会一起变慢或暂停。

### 最大差异时间

*最大差异时间*（max deltatime，默认 0.2 秒）是实时差异时间的上限。当某一帧因为断点调试、拖动窗口、同步加载资源等原因耗时很长时，下一帧的差异时间会被截断为该值，避免物体一步“跳”出很远、物理穿透之类的问题。

* 它约束的是**缩放之前**的值：时间缩放为 2 时，读到的差异时间最大可达 0.4 秒；
* 被截断的那部分时间直接丢弃——发生长帧时游戏时间会落后于真实时间，这是有意的取舍；
* 它同时也是帧同步判定“落后太多、放弃追赶”的阈值（见 [frame.md](frame.md#帧率控制)）。

### 获取与设置

#### Woolang

| 接口 | 说明 |
| --- | --- |
| `je::deltatime()` | 实时差异时间（已乘时间缩放） |
| `je::smooth_deltatime()` | 平滑差异时间（⚠️ 见下文） |
| `u->get_timescale()` / `u->set_timescale(s)` | 读写宇宙 `u` 的时间缩放（`import je::ecs;`） |
| `u->get_max_deltatime()` / `u->set_max_deltatime(dt)` | 读写宇宙 `u` 的最大差异时间 |
| `je::time()` | 引擎启动以来经过的秒数，不受时间缩放影响 |

⚠️ 注意：`je::deltatime()` / `je::smooth_deltatime()` 依赖“当前正在执行的脚本系统”，只能在 `system!` / `work!` 的函数体中调用；在其他地方（包括 `create`、`on_enable` 等回调）调用会 panic。

⚠️ 注意：当前版本的 `je::smooth_deltatime()` 实际返回的是**实时**差异时间（与 `je::deltatime()` 数值相同，仅精度不同，原因见第二部分），脚本侧暂时拿不到真正的平滑差异时间。

#### C++

| 接口 | 说明 |
| --- | --- |
| `deltatime()` / `deltatimed()` | `game_system` 成员：实时差异时间（已乘时间缩放），float / double 版本 |
| `smooth_deltatime()` / `smooth_deltatimed()` | `game_system` 成员：平滑差异时间（已乘时间缩放） |
| `timescale()` / `timescaled()` | `game_system` 成员：当前时间缩放 |
| `universe.get_real_deltatime()` / `universe.get_smooth_deltatime()` | `game_universe` 上的同类读取（另有 `d` 结尾的 double 版本） |
| `universe.get_timescale()` / `universe.set_timescale(s)` | 读写时间缩放 |

```cpp
struct TimerSystem : public jeecs::game_system
{
    double m_elapsed = 0.0;   // 受时间缩放影响的“游戏时间”

    TimerSystem(jeecs::game_world world) : game_system(world) {}

    void Update()
    {
        m_elapsed += deltatimed();                    // 计时：累加实时差异时间
        const float follow_dt = smooth_deltatime();   // 观感类插值：平滑差异时间
        (void)follow_dt;
    }

    void Pause()  { get_world().get_universe().set_timescale(0.0f); }
    void Resume() { get_world().get_universe().set_timescale(1.0f); }
};
```

### 相关术语

参见[术语表](README.md)：

* 宇宙——计算并持有差异时间的对象（[universe.md](universe.md)）；
* 帧——差异时间度量的就是两帧开始时刻的间隔（[frame.md](frame.md)）；
* 系统、系统更新——差异时间最主要的使用者（[system.md](system.md)）；
* 动画、物理、粒子——同样受时间缩放影响的内置功能（[animation.md](animation.md)、[physics.md](physics.md)、[particle.md](particle.md)）。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。计算位于 `src/jeecs_basic_arch_system.cpp` 的 `Universe::update()`，取值接口为同文件中 `Universe` 的 getter 及其 C ABI 包装（`je_ecs_universe_real_deltatime_get` 等，声明见 `include/jeecs/ecs.hpp`）；脚本绑定位于 `src/jeecs_woolang_towoo_script_basic.cpp` 与 `src/jeecs_ecs_woolang_api.cpp`；时钟实现位于 `src/jeecs_basic_time_sleep.cpp`。

### 计算公式

每帧第 0 步（世界更新与宇宙动作处理）完成之后：

```
current_time = je_clock_time()          // 引擎启动以来的秒数（steady_clock）
_m_real_deltatime   = clamp(current_time - _m_real_current_time, 0, _m_max_deltatime)
_m_smooth_deltatime = (_m_smooth_deltatime * 9 + _m_real_deltatime) / 10
_m_real_current_time = current_time
```

* 平滑值是系数为 0.1 的指数移动平均：一次突变的影响按 0.9ⁿ 衰减，约 22 帧后降到原来的 1/10 以下；
* 平滑值累积的是**截断后、未缩放**的实时值；
* 两个值都在宇宙线程上、所有工作开始之前写入，一帧内的读取因此一致。

### 初值与首帧

| 字段 | 初值 |
| --- | --- |
| `_m_real_deltatime` / `_m_smooth_deltatime` | 1/60 |
| `_m_max_deltatime` | 0.2 |
| `_m_time_scale` | 1.0 |
| `_m_real_current_time` | 0 |

⚠️ 注意：`_m_real_current_time` 从 0 开始，而 `je_clock_time()` 从引擎启动（时钟模块 `bootup`）起计时，所以首帧的实时差异时间是“宇宙首帧之前引擎已运行的时长”经截断的结果——启动耗时超过 0.2 秒时首帧就等于最大差异时间，平滑值也会在随后若干帧内偏大。依赖首帧差异时间的逻辑需要留意。

### 时间缩放的生效位置

时间缩放不参与上述计算，而是在**读取时**乘上：`get_real_deltatime()` 返回 `_m_real_deltatime * _m_time_scale`，`get_smooth_deltatime()` 同理。由此可知：

* 修改时间缩放对之后的读取立即生效，无需等到下一帧，也不会污染平滑值的历史；
* `set_time_scale` 以 `max(0., scale)` 钳制下限；`set_max_deltatime` 没有任何校验；
* 最大差异时间只约束缩放前的值（与 `je_ecs_universe_max_deltatime_set` 的注释一致），所以读到的值上限是 `max_deltatime × timescale`；
* 帧同步的休眠与“落后”判定使用未缩放的时钟与 `_m_max_deltatime`，时间缩放不影响帧率。

### 脚本侧的取值路径

`je::deltatime()` → `wojeapi_deltatime` → `current_script_game_system_instance->deltatime()`（float）。`current_script_game_system_instance` 是 `thread_local` 指针，只在 `ToWooBaseSystem::update_step_work` 执行期间指向当前实例（见 [system.md](system.md#towoo-脚本系统的包装)），为空时以 panic 返回。

⚠️ 注意：`wojeapi_smooth_deltatime` 调用的是 `deltatimed()`——实时值的 double 版本——而不是 `smooth_deltatimed()`，这就是第一部分那条警告的来源。

宇宙级的 `get_frame_deltatime` / `get_max_deltatime` / `get_timescale` 等 Woolang 绑定直接转发到对应的 C ABI，不依赖脚本系统上下文，可以在任何地方调用。

### 与帧同步的关系

帧同步决定相邻两帧开始时刻的间隔，差异时间只是事后测量它（见 [frame.md](frame.md#宇宙线程上的一帧)）：

* 主动限速时（帧间隔大于 0），实时差异时间稳定在帧间隔附近；垂直同步时，它接近显示器的刷新间隔；
* 测量点位于第 0 步之后，而休眠位于第 2、3 步之间，因此提交任务（画面呈现）与下一帧第 0 步的耗时，都计入下一帧的差异时间；
* 最大差异时间同时充当截断上限与帧同步的落后阈值：放宽它会让长帧之后的“追赶”更积极，也让单帧的差异时间可能更大。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 差异时间计算、帧同步、时间字段初值 | `src/jeecs_basic_arch_system.cpp`（`Universe::update`、`Universe` 构造函数） |
| 缩放后的取值、`set_time_scale` 下限 | 同上（`get_real_deltatime`、`get_smooth_deltatime`、`set_time_scale`） |
| C ABI 与注释 | `include/jeecs/ecs.hpp`（逐项说明见 [doc/capi](../capi/README.md) 的 ecs 分册） |
| C++ 封装 | `include/jeecs/game.hpp`（`game_system`、`game_universe`） |
| 脚本侧 `je::deltatime` / `je::smooth_deltatime` | `je/je.wo`、`src/jeecs_woolang_towoo_script_basic.cpp` |
| 宇宙级时间设置的 Woolang 绑定 | `je/ecs.wo`、`src/jeecs_ecs_woolang_api.cpp` |
| 时钟 `je_clock_time` | `src/jeecs_basic_time_sleep.cpp`、`include/jeecs/clock.hpp` |
