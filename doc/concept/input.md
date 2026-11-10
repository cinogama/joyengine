# 输入（Input）

本文档是[术语表](README.md)中「输入」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述输入的查询方式与虚拟手柄的用法；
* **第二部分·底层实现**——面向引擎维护者，描述上述行为在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 输入是什么

输入（Input）子系统负责收集键盘、鼠标与手柄的输入状态，供游戏逻辑逐[帧](frame.md)查询。它本身不是[系统](system.md)：键盘与鼠标的状态由窗口事件即时维护，游戏逻辑在系统更新中随时可查；唯一的例外是*虚拟手柄*——它由输入系统（类型名 `Input::VirtualGamepadInputSystem`）在每帧的早期阶段从键盘状态合成。

### 键盘

按键用 `keycode` 枚举表示，可打印字符（字母、数字、空格、标点）的枚举值就是其 ASCII 码；其余按键包括修饰键（`L_SHIFT` 等）、`TAB` / `ENTER` / `ESC` / `BACKSPACE`、小键盘区（`NP_*`）、方向键与 `F1`~`F16`，另有自定义键位。Woolang 中空格键的枚举名是 `keycode::_`。

⚠️ 注意 查询接口返回的是**按住状态**。引擎不提供“本帧刚按下 / 刚松开”的事件式查询——需要边沿检测时，请把上一帧的状态存在脚本系统的上下文里自行比较（示例见下文）。

### 鼠标

鼠标状态按*组*（group）区分，共 16 组：桌面平台只使用第 0 组；触摸屏平台（Android、WebGL）把每一根手指作为一个组上报，多点触控因此可以直接用组号区分。每组各自维护：

* 按键状态（`mousecode`：左 / 中 / 右与自定义键位）；
* 坐标：`mousepos` 返回窗口像素坐标；`mouseviewpos` 返回屏幕空间坐标——x、y 各自归一到 −1~+1，y 向上为正，常用于[射线](ray.md)拾取；
* 滚轮计数 `wheel`：随滚动累积，负值表示向另一方向滚动。

坐标的更新不会移动真实光标，只记录位置。`input::set_lock_mouse(true)` 可以请求锁定（隐藏并禁用）鼠标。

### 手柄

手柄采用**虚拟手柄**抽象：所有手柄（包括物理手柄）都统一为 `je_io_gamepad_handle_t` 句柄，查询接口只有一套。

* *按键*：`gamepadcode` 枚举——方向键（UP/DOWN/LEFT/RIGHT）、A/B/X/Y、肩键与摇杆按下（LB/RB/LS/RS）、SELECT/START/GUIDE；
* *摇杆轴*：`joystickcode` 枚举——左/右摇杆（L/R）返回模长不超过 1 的 (x, y)，**y 向上为正、x 向右为正**；扳机键（LT/RT）借用同一接口，只使用 x 值表示按压深度（0~1），y 恒为 0；
* `gamepad::get_all()` 列出全部手柄；`gamepad::last()` 返回最近操作过的手柄——被操作的判定是“最后上报状态的时间”，`actived` 可以查到这个时间戳。

物理手柄的连接与断开会自动反映到手柄列表中。

### 虚拟手柄（VirtualGamepad）

`Input::VirtualGamepad` 组件把键盘按键映射为一个手柄：组件构造时就会创建一个虚拟手柄，输入系统每帧把映射到的按键状态写入这个手柄。这让同一套手柄逻辑天然兼容键盘操作。

构造时的**默认映射**：

| 键盘 | 手柄 |
| --- | --- |
| 方向键 UP / DOWN / LEFT / RIGHT | 方向键 UP / DOWN / LEFT / RIGHT |
| ENTER | START |
| ESC | SELECT |
| W / A / S / D | 左摇杆（上 / 左 / 下 / 右） |

摇杆是合成的：按住 W 时左摇杆 y 为 +1，同时按住 W 与 D 时为归一化后的对角方向。按键映射存在成员 `keymap` 中（WASD 摇杆另有独立成员 `left_stick_up_left_down_right[4]`），可以按需修改——但该组件没有注册任何反射成员，因此只能在 C++ 中改写，Woolang 与编辑器暂无法访问其内容。

同一手柄的状态对所有查询者可见：`gamepad::last()` 会把“最近被操作的虚拟手柄”与物理手柄一并纳入选择。

### 读取时机

输入状态在窗口事件到来时立即更新，随帧被消费：

* 游戏逻辑（`PRE_UPDATE!` / `UPDATE!` / `LATE_UPDATE!`）读到的都是**当前最新**的按住状态；
* 虚拟手柄的合成发生在 `StateUpdate` 阶段（早于脚本的 `UPDATE!`，见[世界](world.md#系统更新的八个阶段)），因此 UPDATE 阶段读到的是本帧的合成结果；
* 滚轮计数没有自动清零，由各平台按事件语义维护，逻辑侧消费即可。

### API 速览

#### Woolang（`je::input`）

```woolang
UPDATE!
{
    // “刚按下”需要自己与上一帧比较：引擎只提供按住状态
    work! detect_jump()
    {
        let down = input::keydown(keycode::_);   // keycode::_ 即空格键
        if (down && !context.m_space_was_down)
            std::println("jump!");
        context.m_space_was_down = down;
    }

    // 用最近操作过的手柄（包括 VirtualGamepad 模拟出的手柄）驱动角色
    system! move(
        vel: Physics2D::LinearVelocity,
    )
    {
        match (input::gamepad::last())
        {
            value(pad)?
            {
                let (x, _) = pad->axis(joystickcode::L);
                let v = vel.value->get;
                vel.value->set((x * 5.0, v[1]) as vec2);
                if (pad->button(gamepadcode::A))
                    std::println("A");
            }
            none?;
        }
    }
}
```

键盘与鼠标的其它查询：`input::mousedown(group, mousecode)`、`input::mousepos(group)`、`input::mouseviewpos(group)`、`input::wheel(group)`，以及窗口信息 `input::windowsize()` / `windowpos()`。

#### C++（`jeecs::input`，头文件 `jeecs/input.hpp`）

C++ 侧提供同名内联函数与 `input::gamepad` 包装类；另有三个用于边沿检测的辅助宏 `is_up(state)`、`first_down(state)`、`double_click(state, interval)`——它们按调用点缓存上一帧状态，只在按键或鼠标状态的处理循环中使用：

```cpp
using namespace jeecs;

void MySystem::Update()
{
    if (input::keydown(JE_KEY_ENTER))
        debug::loginfo("enter holding");

    auto mpos = input::mousepos(0);         // ivec2，窗口像素坐标
    auto view = input::mouseviewpos(0);     // vec2，屏幕空间 -1~+1

    if (input::mousedown(0, JE_MOUSE_LEFT))
        debug::loginfo("left button holding");

    for (auto& pad : input::gamepad::all())
    {
        auto stick = pad.stick(JE_JOY_L);   // vec2，模长 <= 1
        bool a = pad.button(JE_GAMEPAD_A);
    }
}
```

### 相关术语

参见[术语表](README.md)：

* [帧](frame.md)、[系统更新](system.md)——输入被读取的时机；
* [射线](ray.md)——`mouseviewpos` 屏幕坐标的典型用途；
* [脚本](script.md)——`work!` / `system!` 的声明方式。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。IO 状态与虚拟手柄位于 `src/jeecs_io.cpp`（C ABI 见 `include/jeecs/io.hpp`），键盘鼠标事件的接入在 `src/jeecs_graphic_api_interface_glfw.hpp`（桌面）与 `android/`、`webgl/`（触摸），虚拟手柄系统位于 `src/jeecs_core_input_system.cpp` / `.hpp`，C++ 辅助位于 `include/jeecs/input.hpp`，Woolang 绑定见 `je/input.wo` 与 `src/jeecs_input_woolang_api.cpp`。

### 全局状态与状态更新

`jeecs::io` 匿名命名空间中的 `GlobalState` 持有全部输入状态：按键状态表、16 组鼠标状态（按键、位置、滚轮）、窗口尺寸与位置、待处理的窗口标题/尺寸修改请求，以及手柄实例列表。`je_io_*_update` 系列接口直接写这些字段——它们描述的是“引擎观察到的输入”，与真实设备解耦：

* 桌面平台：GLFW 的键盘、鼠标、滚轮、窗口回调把事件翻译成 `je_keycode_t` / `je_mousecode_t` 后调用 update；可打印字符的 GLFW 键码与 ASCII 一致（实现里有 `static_assert(GLFW_KEY_A == 'A')`）；
* 触摸平台：WebGL 与 Android 端把触摸点映射为鼠标组（单点用组 0，多点以触摸 id 为组号），同样走 update 接口；
* 逆向通道：`je_io_window_size_set` / `je_io_window_title_set` 只置标记，由图形后端在 `update()` 中经 `fetch_update` 取走并真正调整窗口；鼠标锁定同理。

滚轮按事件累加：GLFW 滚轮回调先读旧值再加上本次增量，负值表示另一方向。

### 虚拟手柄的存储

`je_gamepad_state_t` 保存名称、GUID、启用标记、最后操作时间戳、按 `JE_GAMEPAD_COUNT` 索引的按键状态和按 `JE_JOY_COUNT` 索引的摇杆状态（位置 + 死区）。实例放在全局链表中，关闭的实例进入空闲槽队列复用。摇杆更新时会做**死区与模长处理**：模长不超过死区时视为 0；超过 1 时按模长归一化。任何状态写入都会刷新最后操作时间，这就是 `gamepad::last()` 的判定依据。

### 物理手柄的接入

GLFW 后端为每个物理手柄创建一个虚拟手柄实例（以设备名与 GUID 标识），连接/断开由 GLFW 的 joystick 回调驱动。手柄状态不在事件回调中更新，而是在图形后端每次 `update()` 时统一 `glfwPollEvents()` 后逐个 `glfwGetGamepadState` 轮询，把按键与摇杆写入对应实例——扳机键按 `je_joystickcode_t` 的语义只占用 x 分量。轮询发生在图形更新的开始，因此本帧的窗口事件（键盘、鼠标、手柄、关闭请求）在下一次系统更新前都已就绪。

### VirtualGamepadInputSystem：键盘到手柄的合成

输入系统只实现 `StateUpdate`（第 2 阶段），早于脚本的 `UPDATE!`（第 3 阶段）与其它依赖输入的系统。对每个带 `Input::VirtualGamepad` 的实体：

1. 读取 `left_stick_up_left_down_right` 中的四个键，按“上 +y、左 −x、下 −y、右 +x”合成左摇杆并写入 `je_io_gamepad_stick_update`（对角输入会被上述归一化处理为单位向量）；
2. 遍历 `keymap`，把映射到的键盘按住状态**按手柄键聚合**（多个键盘键映射同一手柄键时取或）后写入 `je_io_gamepad_button_state_update`。

组件的拷贝与移动语义值得注意：拷贝构造会**创建新的手柄实例**（复制映射表），移动构造则转移句柄并把源置空；析构时关闭句柄。因此由[预制体](prefab.md)实例化出的每个实体都有独立的手柄。

### 边沿检测辅助宏

`is_up` / `first_down` / `double_click` 是对函数模板 `jeecs::input::_isUp` 等的宏包装，以“文件名哈希 + 行号”作模板参数，让**每个调用点**拥有独立的静态状态——同一帧内对同一按键多次调用，或不同调用点观察同一按键，都互不干扰。这也意味着它们只适合按固定按键、逐帧调用；把它们放进循环里遍历按键是错误用法。

### Woolang 绑定

`je/input.wo` 暴露枚举（`keycode` / `mousecode` / `gamepadcode` / `joystickcode`）与查询函数；`je/internal.wo` 另有 `input::internal::update_*` 系列供平台胶水层写状态。`gamepad` 是 gchandle，包装 C++ 的 `input::gamepad` 实例，GC 回收时随之析构（句柄类不持有手柄所有权，关闭手柄由创建方负责）。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 输入码枚举与 C ABI | `include/jeecs/io.hpp`（逐函数说明见 [doc/capi/io](../capi/io/)） |
| 输入状态、虚拟手柄存储与更新 | `src/jeecs_io.cpp` |
| VirtualGamepad 组件 | `include/jeecs/components.hpp`（`namespace Input`） |
| 虚拟手柄输入系统 | `src/jeecs_core_input_system.cpp` / `.hpp` |
| C++ 查询辅助、gamepad 类、边沿检测宏 | `include/jeecs/input.hpp` |
| 键鼠事件接入（GLFW） | `src/jeecs_graphic_api_interface_glfw.hpp` |
| 触摸/其它平台接入 | `webgl/main.cpp`、`android/app/src/main/cpp/main.cpp` |
| Woolang 绑定 | `je/input.wo`、`je/internal.wo`、`src/jeecs_input_woolang_api.cpp` |
