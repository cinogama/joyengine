# 帧（Frame）

本文档是[术语表](README.md)中「帧」与「工作」两个词条的详细说明——工作是宇宙在一帧中调用逻辑的基本单位，两者合并在本文中介绍。内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述一帧的构成、帧率控制，以及工作的两种形态与三个时机；
* **第二部分·底层实现**——面向引擎维护者，描述宇宙线程上一帧的完整执行顺序、工作的存储与调用，以及画面帧与逻辑帧同步的实现。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 帧是什么

JoyEngine 采用**画面帧与逻辑帧同步**的模型。宇宙（见 [universe.md](universe.md)）在自己的更新线程上循环，每一轮循环依次调用所有注册到宇宙中的*工作*（Job），**完成一轮工作调用即为一帧**。所有系统的更新逻辑，都是在常规任务中作为一项工作被执行的。

开启了图形管线的宇宙，会在每个逻辑帧的末尾把画面交给图形线程绘制，并等待绘制完成——一个逻辑帧恰好对应一个画面帧，游戏逻辑的帧率就是画面的帧率。

### 一帧的四个步骤

```
0. 世界更新   应用上一帧提交的结构变更；处理宇宙动作（加入世界、注册/注销工作）
   ── 计算本帧的差异时间 ──
1. 预先任务   预先时机的工作
2. 常规任务   常规时机的工作：系统更新在这里执行（所有世界并行）
   ── 帧同步：开启主动限速时，在这里休眠到本帧的预定时刻 ──
3. 提交任务   提交时机的工作：画面呈现在这里
```

* 第 0 步是两帧之间的同步点：系统在第 2 步提交的结构变更（增删组件、实体、系统等）要到下一帧的第 0 步才生效，世界视角的细节见 [world.md](world.md#一帧之中世界何时被更新)；
* 差异时间在第 0 步之后、第 1 步之前计算，一帧之内读到的差异时间不变（除非期间修改了时间缩放，见 [deltatime.md](deltatime.md)）；
* 第 2 步中的系统更新又细分为八个阶段，见 [system.md](system.md#系统更新的八个阶段)。

### 帧率控制

两帧之间的节奏由以下两种机制之一决定：

* **垂直同步**：图形配置中的帧率为 0 时（默认）启用垂直同步——提交任务中的画面呈现会等待显示器刷新，帧率因此与刷新率一致。此时宇宙的帧间隔被设为 0，不再额外休眠；
* **主动限速**：宇宙的帧间隔大于 0 时（未开启图形管线的宇宙默认为 1/60 秒），宇宙在常规任务完成后休眠到本帧的预定时刻，再进入提交任务。预定时刻每帧推进一个帧间隔：偶尔落后时，随后几帧会因预定时刻已过而不再休眠，从而追上节奏；若某帧的开始时刻落后预定时刻达到*最大差异时间*，就放弃追赶、从当前时刻重新计时，避免长时间卡顿之后连续多帧不休眠地“补帧”。

```woolang
u->set_frame_deltatime(1. / 30.);    // 主动限速：每帧约 1/30 秒
u->set_frame_deltatime(0.);          // 不主动限速
```

⚠️ 注意：图形线程初始化（以及图形重启）时，会按图形配置的帧率改写宇宙的帧间隔——配置帧率为 0 时设为 0，否则设为 1/帧率。在这之前手动设置的帧间隔会被覆盖。

帧率控制只决定“两帧之间等多久”，并不改变差异时间的计算方式；游戏逻辑应始终用差异时间描述随时间变化的量（见 [deltatime.md](deltatime.md)）。

### 工作是什么

工作（Job）是 JoyEngine 最基础的逻辑调用机制：一个函数加上可选的自定义数据，注册到宇宙之后，宇宙每帧调用它。工作有两种形态：

| 形态 | 调用方式 |
| --- | --- |
| *遍历世界*（for worlds） | 每帧对宇宙中的每个世界各调用一次（世界之间并行）；**停用的世界被跳过** |
| *单独调用*（call once） | 每帧只调用一次，与世界无关 |

以及三个时机：*预先*（pre，第 1 步）、*常规*（normal，第 2 步）、*提交*（after，第 3 步），两两组合共六种。**同一时机内的各个工作并行执行**，时机之间串行。

引擎内置了两项工作：

| 工作 | 形态 · 时机 | 由谁注册 |
| --- | --- | --- |
| 系统更新：对每个世界依次执行八个阶段的系统更新 | 遍历世界 · 常规 | 宇宙创建时自动注册 |
| 画面呈现：请求图形线程绘制本帧，并等待其完成 | 单独调用 · 提交 | 为宇宙开启图形管线时注册 |

术语表说“所有系统的逻辑更新也只是工作的一部分”，指的就是第一项；停用的世界不执行系统更新，也正是因为遍历世界的工作会跳过它。

### 何时直接使用工作

游戏开发中的绝大多数逻辑都应写成系统（见 [system.md](system.md)）。工作是更底层的扩展点，目前只以 C 接口提供（Woolang 与 C++ 封装层都没有包装），适合原生模块（见 [module.md](module.md)）中与具体世界无关、或需要统筹所有世界的逻辑，例如每帧一次的全局服务、跨世界的数据交换（跨世界协作的原则见 [world.md](world.md#多世界的并行执行)）。

使用时注意：

* 注册与注销都经由宇宙的动作队列，**在下一帧开始时生效**；
* 同一时机的工作彼此并行，与系统更新也是并行的（同为常规时机时），工作之间共享的数据需要自行同步；
* 遍历世界的工作在每个世界上的调用也是并行的，回调中只应访问传入的那个世界。

### 相关术语

参见[术语表](README.md)：

* 宇宙——执行帧循环的驱动者（[universe.md](universe.md)）；
* 世界——遍历世界的工作所面对的对象（[world.md](world.md)）；
* 系统、系统更新——常规任务中最主要的工作（[system.md](system.md)）；
* 差异时间——每帧开始时计算的时间量（[deltatime.md](deltatime.md)）；
* 渲染——提交任务中的画面呈现（[render.md](render.md)）。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。帧循环与工作的实现位于 `src/jeecs_basic_arch_system.cpp`（`Universe::update`、`Job`、`update_universe_action_and_worlds`），画面呈现工作位于 `src/jeecs_graphic_host.cpp`，休眠实现位于 `src/jeecs_basic_time_sleep.cpp`，C ABI 见 `include/jeecs/ecs.hpp`。

### 宇宙线程上的一帧

`Universe::update()` 的完整顺序（更新线程如何反复调用它见 [universe.md](universe.md#更新线程与退出流程)）：

```
0. update_universe_action_and_worlds()
     parallel_foreach 世界列表 → world->update()；返回 false 的世界移出列表并 delete
     取出全部宇宙动作并处理（见 universe.md）
   current_time = je_clock_time()
   计算实时 / 平滑差异时间，_m_real_current_time = current_time
   // 源码注释“一个逻辑帧从此处开始”位于这里
1. parallel_foreach _m_shared_pre_jobs
2. parallel_foreach _m_shared_jobs
   if _m_frame_deltatime > 0:
       _m_frame_current_time += _m_frame_deltatime
       je_clock_sleep_until(_m_frame_current_time)
       if current_time - _m_frame_current_time >= _m_max_deltatime:
           _m_frame_current_time = current_time
3. parallel_foreach _m_shared_after_jobs
```

几个容易忽略的细节：

* 休眠放在第 2、3 步之间而不是帧尾：源码注释说明，这是为了在非垂直同步的设定下，让画面在同步时间点到达后立即提交，呈现尽可能平滑；
* “落后太多”的判定用的是本帧**开始**时刻 `current_time`（第 0 步之后采样），而非休眠后的时刻；重置只影响下一帧的预定时刻；
* `je_clock_sleep_until` 转为 `je_clock_sleep_for`：先休眠 `time - sleep_suppression`（提前唤醒量默认 0.0005 秒，可由 `je_clock_sleep_suppression_set` 调整），剩余时间以 `yield` 自旋补足；Windows 上休眠期间用 `timeBeginPeriod(1)` 提高计时器精度。

⚠️ 注意新世界的生效时机：第 N 帧中创建的世界，在第 N+1 帧第 0 步**处理宇宙动作时**才加入列表——此时已有世界的 `update()` 已经执行完毕，新世界的命令缓冲（启用、系统、实体）要到第 N+2 帧的第 0 步才被应用；在第 N+1 帧中它仍是默认的停用状态，所有工作都会跳过它。

### 工作的存储与调用

`Job` 记录形态（`CALL_ONCE` / `FOR_WORLDS`）、两种函数指针的联合体、所属宇宙、`m_custom_data` 与 `m_free_function`；析构时若释放函数非空，以 `custom_data` 调用它。三个时机各自存放在 `_m_shared_pre_jobs` / `_m_shared_jobs` / `_m_shared_after_jobs` 中，这些列表**只在宇宙线程的第 0 步被修改**，因此遍历时无需加锁。

调用是两层并行：外层对该时机的工作列表 `parallel_foreach`；`FOR_WORLDS` 型在内层对世界列表再 `parallel_foreach`，逐个调用 `World::execute_world_job`——它检查 `_m_world_enabled`，**停用的世界在这里被跳过**；`CALL_ONCE` 型直接以 `custom_data` 调用。系统更新工作内部还有第三层：阶段串行、阶段内系统并行（见 [world.md](world.md#系统更新的八个阶段)）。所有 `parallel_foreach` 只在 Release 构建中真正并行，见 [system.md](system.md#系统实例的创建与落库)。

工作以“函数指针 + 形态”识别：注销时不比较 `custom_data`，只删除对应列表中找到的**第一个**匹配项，因此同一函数在同一时机注册两次会得到两个独立的工作。同一帧内的注册/注销按提交的相反顺序处理，见 [universe.md](universe.md#宇宙动作队列)。

### 画面帧与逻辑帧的同步

画面呈现工作是图形管线（`je_graphic_uhost_t`）创建时注册的提交时机单独调用工作 `_update_frame_universe_job`：

1. 调用 `je_graphic_update`：置位更新请求并通知图形同步线程（由 `graphic_syncer_host` 驱动，`jedriver` 中即主线程），随后阻塞；同步模式由 `BRANCH_CHAIN_POOL_SIZE` 决定，当前为 2，因此是 `JE_GL_WAIT_LAST_FRAME_END`——**等到图形线程画完上一帧**即返回，本帧录制可与上一帧绘制并行；
2. 等待结束后回调翻转各渲染分支的链式缓冲；
3. 若 `je_graphic_update` 返回 false（图形已终止，例如窗口被关闭），则停止后续呈现，并对宇宙调用一次 `trim()`（见 [universe.md](universe.md#生命计数)）。

于是宇宙线程每帧都在第 3 步与图形线程会合一次，逻辑帧与画面帧一一对应。呈现放在提交任务、即下一帧的世界更新之前完成的原因，见[世界文档](world.md#宇宙主循环中的一帧)。

帧间隔的改写发生在主线程的 `je_graphic_sync_init`（图形上下文就绪或重启时）：配置的 `m_fps` 为 0 时把宇宙帧间隔设为 0（由垂直同步限速），否则设为 `1.0 / m_fps`。头文件注释约定“不限制帧率请设置为 SIZE_MAX”，此时帧间隔是一个极小的正数，休眠形同虚设。默认图形配置为 640×480 窗口、`m_fps = 0`、MSAA×4。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 帧循环 `Universe::update`、帧同步、差异时间计算 | `src/jeecs_basic_arch_system.cpp` |
| `Job` 结构、工作的注册/注销处理 | 同上（`Job`、`update_universe_action_and_worlds`） |
| 工作注册 C ABI 与说明 | `include/jeecs/ecs.hpp`（逐项说明见 [doc/capi](../capi/README.md) 的 ecs 分册） |
| 画面呈现工作、窗口关闭时的 `trim()` | `src/jeecs_graphic_host.cpp`（`_update_frame_universe_job`） |
| `je_graphic_update` 与同步模式、帧间隔改写 | `src/jeecs_graphic_api_basic.cpp`（`je_graphic_update`、`je_graphic_sync_init`）、`include/jeecs/graphic.hpp` |
| 休眠与计时 | `src/jeecs_basic_time_sleep.cpp`、`include/jeecs/clock.hpp` |
