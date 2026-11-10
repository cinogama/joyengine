# 入口（Entry）

本文档是[术语表](README.md)中「入口」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述引擎如何找到并执行入口脚本、入口脚本里应该做什么；
* **第二部分·底层实现**——面向引擎维护者，描述启动顺序、入口查找与编辑器脚本缓存的实现。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 入口是什么

入口（Entry）是引擎启动后执行的首个[脚本](script.md)。引擎可执行文件（`jedriver`）本身不包含任何游戏逻辑：它完成初始化后就去执行入口脚本，由入口脚本创建[宇宙](universe.md)与[世界](world.md)、注册[系统](system.md)并启用世界——游戏从这里开始运行。

引擎按以下顺序寻找入口：

1. **游戏入口** `@/builtin/main.wo`：运行时目录（启动时即可执行文件所在目录）下的 `builtin/main.wo`，或[资源镜像](fimg.md)中以该路径打包的入口；
2. 找不到游戏入口时，启动内置的[编辑器](editor.md)（`builtin/editor/main.wo`）。

换言之，**可执行文件旁有 `builtin/main.wo` 就启动游戏，没有就启动编辑器**。编辑器的导出向导会生成游戏入口、预编译后打包进资源镜像，所以导出的游戏一启动就直接运行游戏。

### 从可执行文件到入口脚本

```
jedriver 的 main 函数
 └─ 构造引擎上下文（game_engine_context）
     ├─ 初始化引擎各子系统：Woolang 运行时、文件、日志、音频、包与模块、图形……
     ├─ 注册内置组件与基础类型，生成脚本侧的 towoo 声明
     └─ loop()
         ├─ 新线程：执行入口脚本，直到脚本返回
         └─ 主线程：图形同步循环（窗口与渲染上下文）
```

**入口脚本运行在独立的线程上**，主线程留给*图形同步循环*。入口脚本返回、且图形上下文已关闭后，主循环退出，引擎等待入口线程结束、注销类型并关闭各子系统。

### 入口脚本做什么

导出向导生成的入口（模板见 `builtin/editor/Editor/Template/runtime_main.wo.template`）依次完成：

1. 装载项目的原生[模块](module.md)，并保持句柄直到退出；
2. 注册项目的 ToWoo 组件与系统；
3. 装载类型序列化扩展（项目的 `resource/editor/types.wo`）；
4. 设置窗口尺寸与标题；
5. 创建宇宙，并开启该宇宙的图形管线；
6. 加载启动世界并启用；
7. 阻塞等待，直到宇宙退出运行。

手写入口时可以按同样的骨架组织。下面是一个从源码注册组件与系统的最小入口：

```woolang
// builtin/main.wo
import pkg::std;

import je;
import je::ecs;
import je::internal;

using je;

func main()
{
    // 1. 注册 ToWoo 组件 → 刷新 towoo 声明 → 注册 ToWoo 系统
    match (je::towoo::update_component("Player", "@/script/Player.je4component.wo"))
    {
        value(_)?;
        none? je::logerr("注册组件 Player 失败");
    }
    je::towoo::update_api();

    let player_system = je::towoo::update_system(
        "PlayerSystem", "@/script/PlayerSystem.je4system.wo");

    // 2. 创建宇宙并开启图形管线
    let u = universe::create();
    defer u->close();
    je::internal::init_graphic_pipeline_for_editor(u);

    // 3. 创建世界：先装系统，再启用
    let w = world::create(u);
    match (player_system)
    {
        value(t)? do w->add_system(t);
        none? je::logerr("注册系统 PlayerSystem 失败");
    }
    w->set_enable(true);

    // 4. 阻塞直到宇宙退出
    u->wait();
}

main();
```

几条约定：

* **入口脚本不要 import `je::towoo` 或 `je::towoo::components`**：后者在导入阶段就会按名字查找全部组件类型，而此时项目类型还没有注册。入口需要的 `update_api` / `update_component` / `update_system` 都声明在 `je` 中，只导入 `je` 即可（原因详见[脚本](script.md#typeswo-与-componentswo-的生成)）。
* 从源码注册时，**先注册组件、再刷新声明、最后注册系统**，否则系统脚本看不到项目组件。（导出向导生成的入口只在最前面调用一次 `update_api()`，因为导出时项目脚本通常已被预编译为二进制。）
* `init_graphic_pipeline_for_editor` 名字中的 editor 是历史遗留：引擎的图形宿主默认跳过全部绘制，**游戏入口同样必须调用它**，否则画面不会被渲染。
* 世界遵循"先装系统、再启用"的流程（见[世界](world.md#世界的生命周期)）。

### 命令行参数

* `-gapi <名称>`：选择图形后端，可选 `dx11`、`gl3`、`vk120`、`metal`、`none`（仅限编译时启用的后端）。未指定时按 DX11 → Metal → Vulkan → OpenGL 系列 → none 的优先级选择已编译的后端。
* 全部命令行参数同时交给 Woolang 运行时，脚本可以用 `std::cmdlines()` 读取（第 0 项为可执行文件路径）。

### 路径前缀 `@/` 与 `!/`

入口及游戏脚本中的资源路径通常带有以下前缀：

| 前缀 | 含义 |
| --- | --- |
| `@/` | 相对**运行时目录**：启动时为可执行文件目录；编辑器打开项目后被设为项目目录 |
| `!/` | 相对**宿主目录**：引擎可执行文件所在目录，存放 `builtin/` 等引擎内置内容 |

两种前缀都**优先读取真实存在的文件，找不到时再从默认资源镜像中读取**。开发时资源来自项目目录，导出后同样的 `@/` 路径就落到资源镜像里，脚本无需改动。

### API 速览

#### Woolang（`je`、`je::ecs`、`je::internal`）

| 函数 | 说明 |
| --- | --- |
| `je::load_module(name, path)` / `je::unload_module(h)` | 装载 / 卸载原生模块，失败返回 `none` |
| `je::towoo::update_component` / `update_api` / `update_system` | 注册 ToWoo 组件、刷新声明、注册 ToWoo 系统 |
| `je::universe::create()` / `u->wait()` / `u->close()` | 创建宇宙 / 阻塞直到宇宙退出 / 关闭宇宙 |
| `je::internal::init_graphic_pipeline_for_editor(u)` | 为宇宙开启图形管线 |
| `je::internal::set_window_size(w, h)` / `set_window_title(t)` | 设置窗口尺寸与标题 |
| `std::cmdlines()` | 读取命令行参数 |

#### C++（`jeecs::game_engine_context`，头文件 `jeecs/engine.hpp`）

桌面平台的 `jedriver` 只做两件事：

```cpp
int main(int argc, char** argv)
{
    jeecs::game_engine_context context(argc, argv);   // 初始化引擎、注册内置类型
    context.loop();                                    // 执行入口脚本 + 图形同步循环
}                                                      // 析构：等待入口结束、注销类型、关闭引擎
```

需要由平台主循环驱动每一帧的宿主（WebGL、Android）则派生 `game_engine_context`，调用受保护的 `prepare_graphic(true)` 启动入口脚本，再在平台回调中反复调用 `frame()`：

```cpp
class my_host : public jeecs::game_engine_context
{
public:
    my_host(int argc, char** argv)
        : game_engine_context(argc, argv)
    {
        (void)prepare_graphic(true);    // true：同时启动入口脚本线程
    }
    bool tick()                         // 由平台主循环调用
    {
        return frame() != frame_update_result::FRAME_UPDATE_CLOSE_REQUESTED;
    }
};
```

| 方法 | 说明 |
| --- | --- |
| 构造函数 | 初始化引擎，并注册内置组件、基础类型与脚本解析器 |
| `loop()` | 启动入口脚本线程，并在当前线程运行图形同步循环，直到入口结束 |
| `prepare_graphic(execute_entry)`（受保护） | 创建图形同步宿主；`execute_entry` 为真时启动入口脚本线程 |
| `frame()`（受保护） | 推进一帧，返回 `FRAME_UPDATE_NOT_READY` / `READY` / `CLOSE_REQUESTED` |

### 相关术语

参见[术语表](README.md)：

* [脚本](script.md)——入口本身就是一个 Woolang 脚本；
* [编辑器](editor.md)——找不到游戏入口时启动的内置入口；
* [宇宙](universe.md)、[世界](world.md)——入口脚本创建并持有的运行容器；
* [资源镜像](fimg.md)——导出后游戏入口与资源的存放位置；
* [模块](module.md)、[缓存](cache.md)——入口装载的原生扩展；编辑器入口所用的二进制缓存。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。启动流程见 `include/jeecs/engine.hpp`（`game_engine_context`）与 `include/jeecs/graphic.hpp`（`graphic_syncer_host`），入口查找与编辑器缓存位于 `src/jeecs_startup.cpp`，子系统启动顺序定义在 `src/jeecs_basic_bootup.hpp`。

### 启动顺序

`game_engine_context` 的构造函数先调用 `je_core_init(argc, argv)`，它按 `MODULE_LIST` 的顺序展开各子系统的 `bootup`：

```
woolang_runtime → time → file → log → audio → engine_woolang_api
→ external_woolang_packages → external_modules → internal_script
→ graphic::shader_wrapper → core_components_and_systems
→ woolang_runtime::towoo → graphic → ecs
```

`je_core_finish` 按相反顺序调用各 `shutdown`。随后 `entry::module_entry(&types)` 注册内置组件、基础类型与脚本解析器，末尾调用 `je_towoo_api_update()` 生成首份 towoo 声明。

`loop()` 调用 `prepare_graphic(true)` 构造 `graphic_syncer_host`：它注册图形同步回调，并启动 `entry_script_thread` 执行 `je_core_main_script_entry()`；脚本返回时置位 `entry_script_ended` 并唤醒主线程。主线程的 `graphic_syncer_host::loop()` 阻塞等待"有图形上下文就绪"或"入口已结束"：前者进入逐帧的 `frame()`（处理 `JE_GL_SYNC_REBOOT` 重建与 `JE_GL_SYNC_SHUTDOWN` 退出），后者结束循环。析构时先 `delete graphic_syncer`（join 入口线程），再 `module_leave` 注销类型，最后 `je_core_finish`。

`game_engine_context` 只持有类型守卫与图形同步宿主，**不持有任何宇宙**——宇宙由入口脚本创建与关闭。`init_graphic_pipeline_for_editor` 的实现是为该宇宙获取（或创建）图形宿主，并关闭其"跳过全部绘制"开关。

### 入口的查找与执行

`je_core_main_script_entry()` 依次尝试：

1. `je_file_open("@/builtin/main.wo")`：真实文件或默认镜像中的同名条目，读入后交给 `wo_load_binary`——源码与预编译二进制均可加载；
2. 读取编辑器缓存 `@/builtin/editor/main.wo.je4cache`（见下节），从中取出二进制并加载；
3. 从源码编译 `<宿主路径>/builtin/editor/main.wo`。这里刻意使用真实路径而非 `!/` 虚拟前缀，使编辑器源码中相对路径的 import（如 `import GUI::Windows;`）能够被解析；编译成功后用 `woort_CodeEnv_save_binary` 写出缓存。

找到代码后，新建 VM 并以 `woort_bootup(WOORT_IGNORE, cenv, true)` 执行（请求 JIT），结束后关闭 VM、释放代码环境。返回值只表示"是否找到并编译出入口"，不反映脚本是否正常运行完毕。

### 编辑器脚本的二进制缓存

编辑器由大量脚本组成，每次启动都重新编译代价很高，因此首次编译后把整个代码环境序列化为二进制，存成一个[缓存](cache.md)文档（键 `content`，值为二进制 buffer）。读取与写入都以 `EDITOR_SCRIPT_CACHE_VERSION` 同时作为格式版本和"虚拟 CRC"传给缓存管理器——**缓存的有效性与编辑器源码内容无关**，只受缓存管理器主版本与 `EDITOR_SCRIPT_CACHE_VERSION` 控制。

⚠️ 注意：直接修改构建目录下的编辑器脚本不会使缓存失效。CMake 配置阶段的 prebuild 会删除 `build/builtin/editor/main.wo.je4cache`；手动修改编辑器脚本后需要自行删除该文件，缓存格式变化时则应递增 `EDITOR_SCRIPT_CACHE_VERSION`。另外，缓存按 `@/`（运行时路径）读取、按 `!/`（宿主路径）写入，二者在启动时都指向可执行文件目录，因此能够对应上。

### 路径前缀的解析

`je_file_open` 先把路径解析为真实路径：`@` 开头拼接运行时路径（为空时退回宿主路径），`!` 开头拼接宿主路径，其余原样使用。真实文件打开失败且路径带 `@`/`!` 前缀时，再以**原始路径字符串**为键在默认镜像中查找。`file` 子系统启动时把宿主路径、运行时路径都设为可执行文件路径，并从该目录加载默认镜像（`fimg.je4index`，格式见 [fimg](fimg.md)）。平台宿主可以在构造上下文之后、`prepare_graphic` 之前改写这些路径——WebGL 宿主即把两者设为 `/.je4`。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 引擎上下文（构造 / `loop` / `frame`） | `include/jeecs/engine.hpp` |
| 图形同步宿主与入口线程 | `include/jeecs/graphic.hpp`（`graphic_syncer_host`） |
| 入口查找、编辑器缓存、`je_core_init` / `je_core_finish` | `src/jeecs_startup.cpp` |
| 子系统启动 / 关闭顺序 | `src/jeecs_basic_bootup.hpp` |
| `@/` / `!/` 解析与镜像回退 | `src/jeecs_basic_file_image.cpp`、`src/jeecs_basic_file.cpp` |
| `-gapi` 参数解析与默认后端 | `src/jeecs_graphic_api_basic.cpp` |
| 桌面入口 / 平台宿主 | `driver/main.cpp`、`webgl/main.cpp`、`android/app/src/main/cpp/main.cpp` |
| 导出入口模板 | `builtin/editor/Editor/Template/runtime_main.wo.template` |
