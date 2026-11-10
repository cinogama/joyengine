# 编辑器（Editor）

本文档是[术语表](README.md)中「编辑器」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述编辑器的项目结构、界面与工作流；
* **第二部分·底层实现**——面向引擎维护者，描述编辑器的代码组织，以及它如何借助脚本接口驱动引擎。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 编辑器是什么

编辑器（Editor）是随引擎发布的可视化编辑工具。它**本身就是一个 Woolang 程序**：源码位于引擎目录的 `builtin/editor/`，当引擎找不到游戏[入口](entry.md)时作为内置入口启动。界面基于即时模式界面 [GUI](gui.md)（ImGui）绘制；对世界、实体与组件的操作，走的是与游戏[脚本](script.md)相同的 `je` 脚本接口。

编辑器目前仅在 Windows 上提供完整功能。编译项目的 C++ [模块](module.md)需要 CMake：优先使用环境变量 `CMAKE` 指向的可执行文件，否则在 `PATH` 中查找。

### 项目

编辑器启动后先弹出项目创建/打开窗口：选定的目录成为*工作目录*（进程当前目录随之切换），并记入最近打开列表。一个项目目录中的约定文件：

| 路径 | 内容 |
| --- | --- |
| `project.je4project` | 项目文件（TOML），记录项目的原生模块表（模块名 → 路径） |
| `resource/shader`、`resource/font` | 新建项目时从引擎内置副本拷贝的着色器与字体 |
| `resource/editor/types.wo`、`editor.wo` | 类型扩展脚本：各类型的序列化，以及检视面板中的编辑控件 |
| `*.je4world` | 世界文件（TOML） |
| `*.je4component.wo`、`*.je4system.wo` | ToWoo 组件与系统脚本（见[脚本](script.md)） |
| `<名字>.je4module/` | 项目的 C++ 模块（见[模块](module.md)） |
| `.temp/`、`.export/` | 编辑器的临时数据与导出产物（新建项目的 `.gitignore` 已忽略它们与 `*.je4cache`） |

打开项目时，编辑器把运行时路径设为项目目录（此后 `@/` 即指向项目内的文件），扫描并注册全部 ToWoo 脚本，编译并装载全部 C++ 模块，再装载类型扩展脚本。**只有全部模块都编译、装载成功，才会恢复上次退出时打开的世界**——世界的恢复依赖模块注册的类型。编辑器还会为 VSCode 的 Woolang 插件 Wooly 写入 `.vscode/settings.json`，使项目脚本中的 `import je;`、`import pkg::std;` 能被正确解析。

### 界面与工作流

#### 主要面板

| 面板 | 作用 |
| --- | --- |
| 世界编辑视口 | 每个打开的世界一个窗口：游走摄像机、实体拾取、变换游标，以及碰撞体、2D 光源、粒子、UI 元素的可视化编辑手柄 |
| 实体列表 | 当前世界的实体：新建（可勾选常用组件组合）、复制粘贴；把实体拖到另一实体上即挂接为子实体，拖到资源管理器即保存为 `.je4entity` 预制体 |
| 检视面板 | 选中实体的组件与成员编辑；成员控件来自类型扩展脚本 `editor.wo` |
| 材质编辑器 | 实体的模型、着色器列表与 uniform、纹理通道 |
| 系统列表 | 管理当前世界的两组系统（见下） |
| 项目资源管理器 | 浏览项目文件；新建动画、瓦片地图、物理配置、ToWoo 组件/系统、C++ 模块等；把资源拖放到其他面板 |
| 日志浏览器、Woolang REPL | 查看引擎日志；交互式求值 Woolang 代码 |
| 动画编辑器、瓦片地图编辑器 | 编辑 `.je4animation`、`.je4tilemap` / `.je4tileset` |

主菜单提供：文件（新建/打开/保存世界、导出项目）、视图（重新打开各面板）、编辑（更新类型 & 编辑器、重新编译 ToWoo）、运行（开始运行，`Shift+F5`）、工具（在资源管理器或 VSCode 中打开项目、更新全部着色器、复制诊断信息）。

#### 两组系统

系统列表把世界中的系统分成两组，并分别保存在世界文件的两个键下：

* **运行时**（`systems`）：直接加入世界，在编辑器中就持续运行（例如图形相关系统）——这意味着它们在编辑期间就会读写场景数据；
* **调试时**（`running_systems`）：编辑时不加入世界，只在开始调试运行时加入。游戏运行时加载世界会把两组系统一并加入。

#### 调试运行（Play in Editor）

「运行 → 开始运行」后，编辑器为当前宇宙的所有世界保存快照，`set_enable(false)` 暂停它们，趁暂停把"调试时"系统加入世界，关闭世界视口，等待数帧后再 `set_enable(true)` 恢复运行（"先装系统、再启用"，见[世界](world.md#启用与停用)）。运行期间默认进入沉浸模式，只保留一个停止条，`Shift+F6` 可切换编辑器界面的显隐。

停止运行时，编辑器关闭宇宙中的全部世界（包括运行中新建的世界），再用快照重建世界与视口。**运行期间对世界的一切改动都不会保留**；运行中的临场编辑发生在临时视口里，拥有各自独立的编辑历史，停止时一并回收。

#### 撤销与重做

对实体的每次修改都封装为一个可撤销的*动作*（Action），按世界视口记录编辑历史；连续的同类改动（如一次拖拽）会被合并为一步。世界存在未保存的修改时，视口会给出标记，关闭前也会询问。

#### 热更新

* ToWoo 脚本：编辑器每隔数秒检查脚本的修改时间，自动重新编译发生变化的脚本；「编辑 → 重新编译 ToWoo」强制全部重编译。
* C++ 模块：在资源管理器中右键模块目录选择「重新构建并载入」，编辑器用 CMake 重新编译后冻结世界、替换动态库，再恢复世界。
* 类型扩展脚本：「编辑 → 更新类型 & 编辑器」重新装载 `types.wo` 与 `editor.wo`。

#### 导出

「文件 → 导出项目...」打开导出向导：选择启动世界、分辨率与镜像分片大小后，在后台以发布配置（CMake 的 RelWithDebInfo）构建全部 C++ 模块、把脚本预编译为二进制、将资源与缓存打包进[资源镜像](fimg.md)，并生成游戏[入口](entry.md) `@/builtin/main.wo`；最后复制引擎可执行文件（以项目名命名）、运行库与许可证。

### 相关术语

参见[术语表](README.md)：

* [入口](entry.md)——找不到游戏入口时，编辑器作为内置入口启动；
* [脚本](script.md)、[模块](module.md)——编辑器负责注册、编译与热更新的两类项目代码；
* [世界](world.md)——调试运行依赖世界的暂停与恢复；
* [GUI](gui.md)——编辑器界面的绘制基础；
* [资源镜像](fimg.md)、[预制体](prefab.md)——导出产物与实体模板。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。编辑器源码位于 `builtin/editor/`（入口 `main.wo`）；编辑器专用的引擎接口集中在 `je/internal.wo`（`je::internal`）与 `je.wo` 中的 `je::towoo::unsafe::dynamic_parser`，后者实现于 `src/jeecs_editor_woolang_api.cpp`。

### 代码组织

```
builtin/editor/
 ├─ main.wo          启动：设置运行时路径、创建编辑器宇宙、注册 panic 钩子、启动窗口协程、打开项目窗口
 ├─ GUI/             通用界面框架：窗口（IWindows 接口 + 协程）、菜单、状态栏、快捷键、拖放、对话框
 ├─ Singleton/       单例与持久化（编辑器宇宙等）
 └─ Editor/
     ├─ Project/                项目文件、ToWoo 收集器、原生模块收集
     ├─ WorldEditorViewport/    世界视口：摄像机、拾取、gizmo、冻结/恢复、窗口生命周期
     ├─ Action/                 可撤销动作（IAction：redo / undo / describe）
     ├─ Saving/                 世界与实体的编辑器侧序列化
     ├─ Animation/、TilemapEditor/  动画与瓦片地图编辑器
     ├─ Template/               新建文件与导出所用的模板
     └─ EntityList、Inspector、Material、SystemList、Browser、DebugRun、ExportProject、ModulePrepare……
```

编辑器全程只使用一个宇宙（`Singleton::Universe`），打开的所有世界都在其中。各窗口以 GUI 协程的形式逐帧运行。

### 编辑器如何驱动引擎

除与游戏相同的 `je` 系列模块外，编辑器还依赖两组接口：

* `je::internal`：世界的全部实体与系统、实体 UID / 名字 / 父子关系 / 预制体路径、panic 钩子、运行时路径、窗口尺寸与标题、日志钩子、资源镜像打包器等，实现分散在各 `src/jeecs_*_woolang_api.cpp` 中；
* `je::towoo::unsafe::dynamic_parser`：类型扩展脚本的装载与调用。`update_types` / `update_editor` 分别编译 `types.wo` 与 `editor.wo`；`refresh` 遍历全部带脚本解析器的类型，在两份脚本中查找 `<类型名>::parser::saving`、`::restoring` 与 `::edit`；`save` / `restore` / `edit` 借类型的脚本解析器在 C++ 数据与 Woolang 值之间转换后调用它们。检视面板、撤销动作与世界存档都建立在这组接口之上。

引擎核心另外注册了几个编辑器专用组件（`src/jeecs_core_editor_components.hpp`）：`Editor::Name`、`Editor::Invisible`（不在编辑器中显示、不被存档，如调试摄像机）、`Editor::EditorWalker`、`Editor::Prefab`（记录预制体路径）与 `Editor::EntityId`（调试用的实体编号）。

这种结构带来的结果是：修改编辑器只需修改 `.wo` 文件、无需重新编译引擎（但要留意编辑器的二进制缓存，见[入口](entry.md#编辑器脚本的二进制缓存)）；项目也能通过自己的类型扩展脚本定制序列化与检视控件。（`je/editor/` 下曾有两个无引用的遗留脚本 `graphic.wo`、`particalbaker.wo`，已于 2026-11 删除。）

### 动作与编辑历史

`Practice` 为每个视口维护动作列表、当前位置与一个操作队列。动作在视口的逐帧更新中落实；执行失败的动作会推迟一帧重试——结构变更要到下一帧才生效（见[世界](world.md#结构变更的生效时机)），刚创建的实体可能还无法操作，再次失败才视为真正失败。动作只保存类型名，执行时再经 `typeinfo::load` 加载，以免类型热重载后句柄失效；增删"运行时"系统的动作会记录整个世界的快照，因为已启用的系统运行期间可能改动任意实体。

### 冻结世界

重载原生模块、重新编译 ToWoo 时，世界不能处于步进中。`FreezeWorlds` 先进入冻结状态（此时创建视口会失败），从宇宙枚举全部世界并把快照与编辑器上下文存入 `SavedEditorWorldContext` 的 `m_frozen_saved`，暂停世界、关闭视口，执行任务后关闭旧世界，让出一帧（世界在下一帧才真正销毁，避免新旧实体编号冲突）再按快照重建世界与视口。调试运行使用同一套采集与重建逻辑，但快照存在独立的 `m_play_saved` 槽位，两者互不覆盖。

⚠️ 注意：快照只包含能被存档的数据。带 `Editor::Invisible` 的实体不会进入快照，调试摄像机等由视口在重建后自行补建。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 编辑器启动 | `builtin/editor/main.wo` |
| 项目文件、ToWoo 与模块的装载 | `builtin/editor/Editor/Project/` |
| 调试运行 / 冻结世界 | `builtin/editor/Editor/DebugRun.wo`、`builtin/editor/Editor/WorldEditorViewport/FreezeWorlds.wo` |
| 动作与编辑历史 | `builtin/editor/Editor/Action/`、`builtin/editor/Editor/Practice.wo` |
| 世界存档（编辑器侧 / 游戏侧） | `builtin/editor/Editor/Saving/`、`builtin/gaming/Saving/` |
| 导出向导、模块构建 | `builtin/editor/Editor/ExportProject.wo`、`builtin/editor/Editor/ModulePrepare.wo` |
| 编辑器专用接口 | `je/internal.wo`、`src/jeecs_editor_woolang_api.cpp` |
| 编辑器专用组件 | `src/jeecs_core_editor_components.hpp` |
