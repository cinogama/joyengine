# GUI（ImGui）

本文档是[术语表](README.md)中「GUI」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述 GUI 的行为约定与用法；
* **第二部分·底层实现**——面向引擎维护者，描述上述行为在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### GUI 是什么

GUI 指引擎对接的*即时模式*界面库 Dear ImGui（`3rd/imgui` 子模块，docking 分支），以及在其上封装的 Woolang 接口 `je::gui`。编辑器的全部窗口、菜单、属性面板都由它绘制；游戏运行时同样可以用它搭建调试面板。

GUI 不是组件，也不属于任何世界：界面代码在 GUI 协程中每帧执行，绘制结果叠加在所有世界的画面之上。

### 即时模式

即时模式（Immediate Mode）意味着**界面不被保存为对象树**：每一帧都要把“这一帧显示什么”重新声明一遍，控件函数在声明的同时返回交互结果——例如 `Button` 只在被点击的那一帧返回 true。计数器的值、窗口的开关等状态由调用方自己的变量保存。

### GUI 与 UI 的区别

| | GUI（ImGui） | [UI](ui.md)（UserInterface 组件） |
| --- | --- | --- |
| 本质 | 即时模式界面库 | 挂在实体上的组件 |
| 数据归属 | GUI 协程自己的变量 | 世界中的组件数据，可随世界保存 |
| 定位方式 | ImGui 窗口布局与停靠 | 锚点 + 枢轴 + 比例，相对摄像机渲染目标 |
| 绘制时机 | 图形后端每帧收尾，叠在全部画面之上 | UI 渲染管线系统，按摄像机逐个绘制 |
| 典型用途 | 编辑器、调试面板、开发工具 | 游戏内界面（菜单、HUD、按钮） |

### 在 Woolang 中绘制界面

`launch_coroutine(f)` 启动一个 *GUI 协程*：引擎在每帧 GUI 更新时恢复它一次，协程画完本帧界面后调用 `yield()` 让出，下一帧从让出处继续；函数返回即结束。

```woolang
import pkg::std;
import je::gui;

using std;
using je::gui;

launch_coroutine(func(){
    let mut count = 0;          // 协程自己的状态，跨帧保留
    for (;;)
    {
        if (Begin("计数器"))
        {
            Text(F"已点击 {count} 次");
            if (Button("点我"))
                count += 1;
        }
        End();                  // 无论 Begin 返回什么都要调用
        yield();                // 本帧界面声明完毕
    }
});
```

要点：

* `Begin` 返回窗口当前是否可见（如折叠时为 false），**`End` 必须无条件配对**——编辑器的窗口框架用 `defer End();` 保证这一点；
* 窗口标题同时是 ImGui 的 ID，同名窗口会合并为一个；
* 新启动的协程从**下一帧**开始执行；
* 引擎纹理可以直接显示：`Image` / `ImageSize` / `ImageUV` 接受 `je::graphic::texture2d`（UV 按引擎约定 V 轴向上，内部会翻转），编辑器的世界视口就是这样显示摄像机渲染到的帧缓冲。

### 常用接口速览（Woolang `je::gui`）

| 接口 | 说明 |
| --- | --- |
| `launch_coroutine(f)` | 启动 GUI 协程 |
| `Begin` / `BeginAttr` / `BeginAttrOpen` + `End` | 窗口（`BeginAttrOpen` 额外返回关闭按钮状态） |
| `Text` / `TextColored` / `Button` / `InputText` 等 | 常规控件，命名与 ImGui 一致 |
| `Image` / `ImageSize` / `ImageUV` / `ImageButton` | 显示引擎纹理 |
| `GetWindowDrawList()` + `AddLine` / `AddRect` / `AddText` 等 | 在窗口上自绘图元 |
| `node_editor::*`、`CodeEditorContext` | 节点编辑器、带语法高亮的代码编辑器 |
| `input::keystate(key)` | 返回 (上一帧是否按下, 本帧是否按下) |
| `register_exit_callback(cb)` / `unregister_exit_callback()` | 窗口请求关闭时询问是否允许，`cb` 返回 false 即拒绝 |
| `set_font(font, latin_font, size)` | 指定界面字体，**仅在 GUI 后端初始化之前调用才生效** |

### 编辑器如何使用 GUI

编辑器（`builtin/editor`）在 `je::gui` 之上搭了一层窗口框架：`JEEditor::GUI::Windows::start()` 启动一个主协程，每帧依次推进所有已打开窗口的处理过程；每个窗口实现 `IWindows` 接口，由框架负责 `BeginAttrOpen` / `End` 配对、菜单栏、快捷键与模态弹窗。状态栏、日志通知等独立功能则直接用 `launch_coroutine` 启动自己的协程。

退出确认也走 GUI：编辑器注册的退出回调总是返回 false 以拦下关闭请求，转而弹出“即将退出编辑器”的确认框。

### C++ 侧

C++ 没有绘制界面的封装。`jeecs/gui.hpp` 只提供把 ImGui 接入图形后端的胶水接口，仅在对接自定义渲染 API 时使用（见第二部分）；界面本身请在 Woolang 侧用 `je::gui` 编写。

### 相关术语

参见[术语表](README.md)：

* [UI](ui.md)——挂在实体上的游戏界面组件，与 GUI 是两个概念；
* [编辑器](editor.md)——GUI 最主要的使用者；
* [纹理](texture.md)、[帧缓冲](framebuffer.md)——可以直接显示在 GUI 窗口中；
* [脚本](script.md)——GUI 协程即普通的 Woolang 函数。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。胶水层实现位于 `src/jeecs_graphic_imgui_basic_impl.cpp`（C API 声明与注释见 `include/jeecs/gui.hpp`），共享状态见 `src/jeecs_graphic_imgui_internal.hpp`，各图形后端的 ImGui 适配位于 `src/jeecs_graphic_imgui_backend_*.cpp`，Woolang 绑定位于 `src/jeecs_gui_woolang_api.cpp` 与 `je/gui.wo`。

### 与图形后端的配对

每个图形实现都有一份配套的 ImGui 适配文件，在图形上下文启动、每帧收尾与关闭时分别调用 `jegui_init_*` / `jegui_update_*` / `jegui_shutdown_*`（声明见 `jeecs_graphic_imgui_backend_api.hpp`）：

| 图形实现 | 适配文件 | 平台层 + 渲染层 |
| --- | --- | --- |
| `jeecs_graphic_api_impl_dx11.cpp` | `..._backend_dx11.cpp` | GLFW + DX11 |
| `jeecs_graphic_api_impl_opengl3.cpp`（GL3.3 / GLES3.0 / WebGL2.0） | `..._backend_opengl3.cpp` | GLFW（Android 下为 EGL + `imgui_impl_android`，见 `..._backend_android_api.cpp`）+ OpenGL3 |
| `jeecs_graphic_api_impl_vk120.cpp` | `..._backend_vk120.cpp` | GLFW（Android 下为 EGL + `imgui_impl_android`）+ Vulkan |
| `jeecs_graphic_api_impl_metal.cpp` | `..._backend_metal.cpp` | GLFW + Metal |
| `jeecs_graphic_api_impl_none.cpp` | `..._backend_none.cpp` | 不绘制；固定 1920×1080、每帧 1/60 秒 |

适配文件先调用通用的 `je_gui_backend_init`，再初始化 ImGui 官方的平台/渲染后端；关闭时先调用 `je_gui_backend_shutdown`，再关闭官方后端并 `DestroyContext`（none 后端只调用前者）。以 DX11 为例，`dx11_commit_update` 先切回默认帧缓冲再调用 `jegui_update_dx11()`——GUI 总是在场景绘制完成之后画，因而位于最上层。

### 初始化：`je_gui_backend_init`

1. 把图形上下文与两个回调存入**线程局部**的 `_je_gui_tls_ctx`，加载 `imgui_image_displayer.shader`；
2. `ImGui::CreateContext()`，开启停靠（Docking）；仅在窗口模式且未使用 EGL 时开启多视口（Viewports）；布局保存在 `<宿主路径>/builtin/imgui.ini.je4cache`；
3. 加载字体：通用字体缺省为 `HarmonyOS_Sans_SC_Regular.ttf`，拉丁字体缺省为 `JetBrainsMono_Regular.ttf`，缺省字号 18。拉丁字体按默认字符区间载入，通用字体以合并模式补齐 CJK 标点、假名、半角与 CJK 表意字符；拉丁字体缺失时改用通用字体的完整中文区间。

`je_gui_backend_font_set` 只把路径与字号写进静态上下文，由下一次 init 读取——这就是“仅在 init 前调用生效”的原因（图形接口重启时 init 会再次执行）。

### 每帧更新：`je_gui_backend_update`

1. 确认 ImGui 图片着色器就绪，`ImGui::NewFrame()`；
2. 铺一个覆盖主视口的透明宿主窗口 “Main DockSpace”（中央节点穿透），作为所有窗口的停靠空间；
3. 刷新按键记录（`input::keystate` 首次查询某键时登记，此后每帧滚动“上一帧/本帧”状态）；
4. 未被停止时恢复全部 GUI 协程：每个协程运行在独立的 VM 上，首次执行为 spawn、之后为 resume；返回 YIELD 的放回队列，其余关闭 VM 并释放；本帧新启动的协程此时才并入队列——所以它们从下一帧开始；
5. `ImGui::Render()`；ImGui 颜色按 sRGB 书写，而交换链是 sRGB 目标、场景着色器输出线性值，因此先把顶点颜色线性化（主视口直接处理，多视口窗口经 `Renderer_RenderWindow` 钩子处理）；
6. 调用 `platform_draw_callback(data)`，由适配文件执行真正的 `RenderDrawData`；
7. 多视口开启时更新并绘制平台窗口。

默认帧缓冲不支持硬件 sRGB 写入控制时，后端在 ImGui 绘制结束后调用 `jegui_commit_srgb_encode_pass`，用 `srgb_encode.shader` 把本帧的线性离屏画面编码后画到默认帧缓冲。

### 引擎纹理如何进入 ImGui

`je_gui_backend_init` 的两个回调由图形实现提供：

* `je_gui_user_image_loader_t`——把 `je_gl_texture_2d_t` 换成后端原生句柄（`je_gui_user_image_handle_t`，即 `uint64_t`），作为 `ImTextureID`。例如 DX11 返回纹理的 ShaderResourceView；
* `je_gui_user_sampler_loader_t`——把指定着色器声明的采样器状态绑定到管线。

`Image` 系列绑定先确保纹理资源已上传，然后向当前窗口的 DrawList 插入回调：绘制图片前用 sampler 回调应用 `imgui_image_displayer.shader` 的采样设置，图片后插入 `ImDrawCallback_ResetRenderState` 恢复 ImGui 自身状态。图片以 UV (0,1)→(1,0) 绘制，即竖直翻转，使引擎纹理正向显示。

### 关闭与退出确认

* `je_gui_backend_shutdown(reboot)`：非重启时关闭全部 GUI 协程的 VM；重启图形接口时协程保留，重新 init 后继续执行；
* `je_gui_backend_shutdown_callback()`：窗口请求关闭时由后端调用（如 DX11 的 `dx11_pre_update` 收到 CLOSE）。未注册退出回调时返回 true；否则在注册时创建的专用 VM 中调用 Woolang 回调，以其返回值决定是否停止图形更新。

⚠️ 注意 退出回调只能注册一个，重复注册会 panic；需要更换时先 `unregister_exit_callback()`。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| C API 声明（init / update / shutdown / font_set 及回调类型） | `include/jeecs/gui.hpp`（逐项说明见 `doc/capi/gui/`） |
| 胶水层与协程调度 | `src/jeecs_graphic_imgui_basic_impl.cpp`、`src/jeecs_graphic_imgui_internal.hpp` |
| 各后端适配 | `src/jeecs_graphic_imgui_backend_{dx11,opengl3,vk120,metal,none,android_api}.cpp` |
| 后端调用点 | `src/jeecs_graphic_api_impl_*.cpp`（`jegui_init_*` / `jegui_update_*` / `jegui_shutdown_*`） |
| Woolang 绑定 | `src/jeecs_gui_woolang_api.cpp`、`je/gui.wo` |
| 编辑器窗口框架 | `builtin/editor/GUI/Windows.wo` |
