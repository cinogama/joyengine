# 宇宙（Universe）

本文档是[术语表](README.md)中「宇宙」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述宇宙的职责、创建与退出流程，以及宇宙级的时间设置；
* **第二部分·底层实现**——面向引擎维护者，描述宇宙的线程模型、动作队列与退出流程的实现。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 宇宙是什么

宇宙（Universe）是容纳所有世界的容器，也是引擎运转的**驱动者**：每个宇宙拥有一条专属的更新线程，在其上一帧接一帧地循环，每一帧依次调用注册到宇宙中的*工作*——其中最重要的一项，就是对所有世界执行系统更新（见 [frame.md](frame.md)）。

* 一个世界属于且仅属于一个宇宙，宇宙内的各个世界并行更新（见 [world.md](world.md)）；
* 差异时间、时间缩放、帧间隔等时间设置都是**宇宙级**的，同一宇宙中的所有世界共享（见 [deltatime.md](deltatime.md)）；
* 图形管线也以宇宙为单位挂接：为宇宙开启图形管线后，它的每一个逻辑帧都与一个画面帧同步（见 [render.md](render.md)）；
* 引擎允许同时存在多个宇宙，原则上不同宇宙之间的数据严格隔离；不过一个程序通常只创建一个宇宙，所以术语表说它“通常是单例”。

### 谁创建宇宙

引擎本身**不会**自动创建宇宙，宇宙由入口脚本创建（见 [entry.md](entry.md)）。导出游戏的启动脚本与编辑器遵循同一个流程：

```woolang
let u = universe::create();          // 创建宇宙，它的更新线程随即开始运行
defer u->close();                    // 离开作用域时（wait 返回之后）销毁宇宙

je::internal::init_graphic_pipeline_for_editor(u);   // 为该宇宙开启图形管线

// ……创建世界、加载场景（见 world.md）……

u->wait();                           // 阻塞，直到宇宙退出
```

`init_graphic_pipeline_for_editor` 名字中的 editor 是历史遗留，导出的游戏同样需要调用它才能看到画面。

### 宇宙的生命周期

#### 生命计数

宇宙带有一个*生命计数*，**创建时自带一次计数**：

* `grow()` 使计数加一，`trim()` 使计数减一；
* **计数归零时宇宙开始退出**（过程见下节）；
* 对已经归零的宇宙再调用 `grow()` / `trim()` 会触发致命错误——宇宙一旦开始退出就不能被“救回来”。

创建时自带的那一次计数，通常由图形管线代为消耗：**用户关闭游戏窗口时，图形管线会对宇宙调用一次 `trim()`**，于是窗口关闭即宇宙退出。编辑器则通过 GUI 的退出回调否决窗口的关闭请求，待自己的关闭流程走完（只剩最后一个窗口）后，才对宇宙调用 `trim()`。

当有额外的参与者需要共同决定宇宙何时退出时（例如一项必须完成的收尾工作），由它先 `grow()`，完成后再 `trim()`，宇宙就会等到所有参与者都放手才退出。

#### 退出的过程

1. 宇宙请求销毁其中的所有世界（世界的销毁流程见[世界的生命周期](world.md#世界的生命周期)），只要还有世界存活，就继续一帧帧地运行；
2. 所有世界关闭后，处理剩余的宇宙动作，并按注册的**相反顺序**调用退出回调——图形管线就在这一步随宇宙释放；
3. 注销并释放所有工作，更新线程结束。

由于宇宙要等**所有**世界关闭才退出，在退出过程中新创建的世界也会被继续销毁、拖长退出过程——因此不要在组件或系统的析构函数中做多余的逻辑，析构只应释放资源。

#### 等待与销毁

* `wait()` 阻塞调用线程，直到宇宙退出；入口脚本通常在最后调用它，避免脚本过早结束。对已经停止的宇宙调用它也无害——退出回调的注册在更新线程退出后会失败，`wait()` 此时直接返回。
* `close()`（C++：`game_universe::destroy_universe`）**无视生命计数**，立即要求宇宙退出，并阻塞到宇宙完全销毁。通常的写法是先 `wait()` 再 `close()`，如上文示例中 `defer` 的用法。

### 宇宙级的时间设置

| 设置 | 默认值 | 说明 |
| --- | --- | --- |
| 帧间隔（frame deltatime） | 1/60 秒 | 期望的两帧间隔，用于主动限速；≤ 0 表示不主动限速（见 [frame.md](frame.md#帧率控制)） |
| 最大差异时间（max deltatime） | 0.2 秒 | 差异时间的上限，也是帧同步判定“落后太多”的阈值 |
| 时间缩放（timescale） | 1.0 | 差异时间的缩放系数，负数按 0 处理 |

它们的含义与计算方式见 [deltatime.md](deltatime.md)。

### 遍历世界

`get_all_worlds()` 返回调用时刻宇宙中的世界列表快照。⚠️ 注意：新创建的世界要到**下一帧开始**时才加入宇宙的世界列表，在此之前不会出现在结果中；反之，已请求销毁的世界在下一帧真正移除之前仍会出现。

### API 速览

#### Woolang（`je::universe`，`import je::ecs;`）

| 函数 | 说明 |
| --- | --- |
| `universe::create()` | 创建宇宙（更新线程立即开始运行） |
| `u->close()` | 无视生命计数立即退出并销毁（阻塞） |
| `u->wait()` | 阻塞直到宇宙退出 |
| `u->grow()` / `u->trim()` | 生命计数加一 / 减一（归零即开始退出） |
| `u->get_frame_deltatime()` / `u->set_frame_deltatime(dt)` | 读写帧间隔 |
| `u->get_max_deltatime()` / `u->set_max_deltatime(dt)` | 读写最大差异时间 |
| `u->get_timescale()` / `u->set_timescale(s)` | 读写时间缩放 |
| `u->get_all_worlds()` | 列出宇宙中的所有世界 |
| `w->get_universe()` | 取世界所属的宇宙 |

#### C++（`jeecs::game_universe`，头文件 `jeecs/game.hpp`）

| 方法 | 说明 |
| --- | --- |
| `game_universe::create_universe()` | 创建宇宙 |
| `game_universe::destroy_universe(u)` | 无视生命计数立即退出并销毁（阻塞） |
| `u.wait()` | 阻塞直到宇宙退出 |
| `u.grow()` / `u.trim()` | 生命计数加一 / 减一 |
| `u.create_world()` | 在宇宙中创建世界 |
| `u.get_timescale()` / `u.set_timescale(s)` | 读写时间缩放（另有 `double` 版本 `get_timescaled` / `set_timescaled`） |
| `u.get_real_deltatime()` / `u.get_smooth_deltatime()` | 实时 / 平滑差异时间（已乘时间缩放） |
| `world.get_universe()` | 取世界所属的宇宙 |

C++ 封装目前没有提供帧间隔与最大差异时间的读写，需要时只能直接使用底层接口。

```cpp
using namespace jeecs;

game_universe universe = game_universe::create_universe();
game_world    world    = universe.create_world();

world.add_system<MyGameSystem>();
world.set_enable(true);

universe.wait();                              // 阻塞到宇宙退出
game_universe::destroy_universe(universe);    // 释放宇宙
```

### 相关术语

参见[术语表](README.md)：

* 世界——宇宙所容纳的内容单位（[world.md](world.md)）；
* 帧、工作——宇宙驱动世界的方式（[frame.md](frame.md)）；
* 差异时间——宇宙负责计算的时间量（[deltatime.md](deltatime.md)）；
* 入口——创建宇宙的脚本（[entry.md](entry.md)）；
* 渲染——以宇宙为单位挂接的图形管线（[render.md](render.md)）。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。实现主体是 `src/jeecs_basic_arch_system.cpp` 中的 `Universe` 类；C ABI 与行为注释见 `include/jeecs/ecs.hpp`，Woolang 绑定见 `je/ecs.wo` 与 `src/jeecs_ecs_woolang_api.cpp`，引擎上下文见 `include/jeecs/engine.hpp`。

### Universe 类

`Universe` 继承空结构体 `je_game_universe_t`（与 `World` 相同的 C ABI 暴露方式，见[世界文档](world.md#world-类与-c-abi)），主要成员：

* `_m_world_list`——世界列表，**只在宇宙线程上修改**；
* `_m_shared_pre_jobs` / `_m_shared_jobs` / `_m_shared_after_jobs`——三个时机的工作列表（见 [frame.md](frame.md#工作的存储与调用)）；
* `_m_universe_actions`——待处理的宇宙动作（见下文）；
* `_m_universe_life`——生命计数（`atomic_size_t`，初值 1）与 `_m_universe_update_thread_stop_flag`——停止标记；
* 时间字段：`_m_frame_deltatime`（1/60）、`_m_real_deltatime` / `_m_smooth_deltatime`（1/60）、`_m_max_deltatime`（0.2）、`_m_time_scale`（1.0），以及两个时间锚点 `_m_frame_current_time` / `_m_real_current_time`（0）；
* `m_exit_callback_list`——退出回调。

另有静态登记表 `_m_alive_universes` 记录所有存活的宇宙。构造函数登记自身，**以普通 FOR_WORLDS 工作的形式注册系统更新工作**（注册同样经过动作队列，因此在第一帧的第 0 步才真正加入），然后启动更新线程 `_m_universe_update_thread`。

### 宇宙动作队列

创建世界、注册/注销工作都不直接修改上述列表，而是向 `_m_universe_actions` 追加一个 `universe_action`。它是 `basic::atomic_list`——基于 CAS 的无锁栈，任意线程都可以追加。动作在每帧第 0 步、所有世界完成 `update()` 之后被一次性取出处理（`update_universe_action_and_worlds`）：

* `ADD_WORLD`——把世界加入 `_m_world_list`（`World` 对象在 `create_world` 时就已构造，只是尚未入列）；
* `ADD_*_JOB`——构造 `Job` 并追加到对应列表；
* `REMOVE_*_JOB`——在对应列表中查找**第一个**函数指针与类型（FOR_WORLDS / CALL_ONCE）都相同的工作并删除（`Job` 析构时以 `custom_data` 调用其释放函数）；找不到时只记录错误日志。

⚠️ 注意：`atomic_list::pick_all` 取出的是栈，遍历顺序为**后提交者先处理**。同一帧内先注册、后注销同一个工作，会先处理注销（找不到而报错）、再处理注册，结果工作依然存在；同一帧创建的多个世界，入列顺序也与创建顺序相反。

### 更新线程与退出流程

更新线程的主体如下（`update()` 即一帧，展开见 [frame.md](frame.md#宇宙线程上的一帧)）：

```
loop:
    update()
    if 停止标记:
        if 世界列表为空: break
        对每个世界 je_ecs_world_destroy()      // 每帧重复请求，直到列表清空
do:
    update()                                   // 处理剩余动作
    依次调用退出回调（栈序：后注册者先调用）
while 仍有未处理的宇宙动作                      // 回调可能追加新动作
delete 全部工作
```

停止标记的来源有三个：`trim_lifetime` 使计数归零；`~Universe`（即 `je_ecs_universe_destroy`，因此无视计数）；引擎关闭时 `je_core_finish` 以模块逆序关闭，ECS 模块最先关闭，`Universe::_shutdown_all_universe` 逐个停止并 `join` 仍存活的宇宙。`grow_lifetime` / `trim_lifetime` 用 CAS 循环修改计数，遇到 0 时 `logfatal`。

`je_ecs_universe_loop`（即 `wait`）的实现是注册一个退出回调去唤醒条件变量，然后阻塞等待。退出回调的注册与更新线程退出时的排水由同一把锁保护：线程退出时先置 `_m_universe_exited` 再消费回调表，此后到达的注册会失败，`wait()` 对已停止的宇宙直接返回而不是永远等待。Woolang 侧的 `wait` / `close` 在阻塞期间通过 `woort_vm_swap(nullptr)` 把当前线程与脚本虚拟机解绑，返回后再换回。

### 宇宙与引擎上下文、线程

`driver/main.cpp` 只构造 `jeecs::game_engine_context`（构造时 `je_core_init` 并注册内置类型）并调用 `loop()`；上下文**并不持有宇宙**。`loop()` 创建 `graphic_syncer_host`，于是程序中有三类线程：

| 线程 | 职责 |
| --- | --- |
| 主线程 | `graphic_syncer_host::loop()`：等待图形上下文就绪，反复执行图形同步帧（`je_graphic_sync_update`）直到图形请求关闭，再等待下一个上下文；入口脚本结束后退出循环 |
| 入口脚本线程 | 执行 `je_core_main_script_entry()`：运行入口脚本，脚本创建宇宙，最终阻塞在 `u->wait()` |
| 宇宙更新线程 | 每个宇宙一条，循环执行帧 |

宇宙线程与主线程之间的帧同步由图形管线注册的提交任务完成（见 [frame.md](frame.md#画面帧与逻辑帧的同步)）。图形管线以宇宙为键单例化，并注册一个退出回调，在宇宙退出时释放自身。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| `Universe` 类、`Job`、动作队列、更新线程 | `src/jeecs_basic_arch_system.cpp` |
| `je_ecs_universe_*` C ABI 与行为注释 | `include/jeecs/ecs.hpp`（逐项说明见 [doc/capi](../capi/README.md) 的 ecs 分册） |
| C++ 封装 `game_universe` | `include/jeecs/game.hpp` |
| Woolang 绑定 | `je/ecs.wo`、`src/jeecs_ecs_woolang_api.cpp` |
| 引擎上下文与主线程循环 | `include/jeecs/engine.hpp`、`driver/main.cpp`、`include/jeecs/graphic.hpp`（`graphic_syncer_host`） |
| 窗口关闭时的 `trim()`、图形管线的宇宙级单例 | `src/jeecs_graphic_host.cpp` |
| 创建宇宙的入口脚本参考 | `builtin/editor/Editor/Template/runtime_main.wo.template`、`builtin/editor/main.wo` |
| 编辑器的关闭流程 | `builtin/editor/Editor/Shutdown.wo`、`builtin/editor/Editor/Project/Project.wo` |
