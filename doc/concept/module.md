# 模块（Module）

本文档是[术语表](README.md)中「模块」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述模块的用途、目录约定与写法；
* **第二部分·底层实现**——面向引擎维护者，描述模块如何被发现、静态链接进引擎，以及运行时的装载流程。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 模块是什么

模块（Module）是以 C++ 编写的可选原生扩展，目录名形如 `<名字>.je4module`。模块向引擎加入原生的[组件](component.md)与[系统](system.md)，适用于性能敏感的逻辑，或需要接入第三方 C/C++ 库的功能。模块与引擎核心之间同样只通过 C API 及其 C++ 封装交互。（图形 API 后端不属于模块，而是引擎核心的一部分。）

同一种模块目录有两种用法：

| 用法 | 存放位置 | 构建方式 | 生效方式 |
| --- | --- | --- | --- |
| **静态链接进引擎** | 引擎源码的 `module/<名字>.je4module/` | 随引擎一起构建，需开启 CMake 选项 `JE4_STATIC_LINK_MODULE_AND_PKGS`（默认开启） | 脚本以模块名调用 `je::load_module` |
| 项目模块 | 项目目录中的 `<名字>.je4module/` | 由[编辑器](editor.md)调用 CMake 构建为动态库 | 编辑器打开项目时装载；导出后由游戏[入口](entry.md)装载 |

术语表中的定义对应第一种用法；第二种（*项目模块*）是编辑器为项目开发提供的形式，两者的目录结构与入口约定完全一致。引擎仓库本身不附带任何模块（`module/` 下的内容被 `.gitignore` 忽略）。

### 目录约定

```
<名字>.je4module/
 ├─ CMakeLists.txt    构建脚本，目标名必须是 <名字>
 ├─ main.cpp          入口：导出 jeecs_module_entry / jeecs_module_leave
 ├─ loader.hpp        类型注册表：注册 components/ 与 systems/ 中的全部类型
 ├─ components/       组件，一个类型一个 <类型名>.hpp
 ├─ systems/          系统，<类型名>.hpp + <类型名>.cpp
 └─ sources/          其他源码
```

在编辑器资源管理器中「新建 → C++ 模块」会生成上述骨架；在模块目录内还可以「新建 → C++ 组件 / C++ 系统」，由对话框生成类型代码。编辑器构建时按 `components/`、`systems/` 中的头文件名生成 `loader.hpp`，把每个类型注册为 `<模块名>::<类型名>`。

### 编写模块

入口文件（编辑器模板）在装载时注册本模块的类型，卸载时注销它们：

```cpp
// main.cpp
#define WOODYN_IMPL

#include "jeecs/typing.hpp"
#include "jeecs/entry.hpp"
#include "loader.hpp"

static jeecs::typing::type_unregister_guard* _module_guard;

extern "C" {
    JE_EXPORT void jeecs_module_entry(woort_Dylib* lib_instance)
    {
        _module_guard = new jeecs::typing::type_unregister_guard();
        jemodule::register_types(_module_guard);    // loader.hpp：注册本模块的组件与系统
        jeecs::entry::module_entry(_module_guard);   // 登记内置类型，并刷新脚本侧的 towoo 声明
    }

    JE_EXPORT void jeecs_module_leave()
    {
        jeecs::entry::module_leave(_module_guard);   // 注销经该守卫注册的全部类型
        delete _module_guard;
    }
}
```

组件是带成员登记函数的普通结构体，系统派生自 `jeecs::game_system`（写法见[组件](component.md)与[系统](system.md)）：

```cpp
// components/Health.hpp
#pragma once
#include "jeecs/typing.hpp"

namespace MyModule
{
    struct Health
    {
        JECS_DISABLE_MOVE_AND_COPY_OPERATOR(Health);
        Health() = default;
        Health(const Health&) = default;
        Health(Health&&) = default;

        float hp = 100.f;

        static void JERefRegsiter(jeecs::typing::type_unregister_guard* guard)
        {
            jeecs::typing::register_member(guard, &Health::hp, "hp");
        }
    };
}
```

### 在脚本中使用模块

模块被装载后，它的组件出现在 `je::towoo::components` 中（如 `MyModule::Health`），系统可以用 `je::typeinfo::load("MyModule::某系统")` 取得类型信息并加入世界。装载句柄需要保持到退出：

```woolang
// 入口脚本中
func main()
{
    let loaded_modules = []mut: vec<handle>;
    defer {
        for (let h : loaded_modules)
            do je::unload_module(h);
    }

    // Windows 下的动态库文件名
    match (je::load_module("MyModule", std::host_path() + "/modules/libMyModule.dll"))
    {
        value(h)? loaded_modules->add(h);
        none? je::logerr("装载模块 MyModule 失败");
    }
    // ……注册 ToWoo 类型、创建宇宙与世界……
}
```

**静态链接的模块同样要以模块名调用 `je::load_module` 才会生效**：引擎启动时只登记了它，入口函数要到这次调用才执行；此时路径参数不会被使用。模块中注册的类型被世界依赖，因此应在加载任何世界之前装载模块。

### 相关术语

参见[术语表](README.md)：

* [组件](component.md)、[系统](system.md)——模块向引擎提供的内容；
* [脚本](script.md)——模块类型在脚本中的使用方式，以及 towoo 声明的刷新；
* [入口](entry.md)——装载模块的典型位置；
* [编辑器](editor.md)——项目模块的创建、构建与热重载；
* [包](package.md)——与模块并列、同样由 `JE4_STATIC_LINK_MODULE_AND_PKGS` 控制静态链接的 Woolang 依赖。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。模块的发现与静态链接位于根 `CMakeLists.txt` 与 `script/prebuild/prebuild.wo`（产物 `src/jeecs_extern_library_module.hpp`），启动期登记见 `src/jeecs_extern_library_module.cpp`，装载与卸载见 `src/jeecs_startup.cpp` 的 `je_module_load` / `je_module_unload`。

### 构建集成

两处分别处理 `module/` 目录：

* **CMake 配置**（仅当 `JE4_STATIC_LINK_MODULE_AND_PKGS=ON`）：强制 `BUILD_SHARED_LIBS=OFF` 后遍历 `module/` 的子目录，对含 `CMakeLists.txt` 的目录执行 `add_subdirectory`；用正则 `^(.*)\.je4module$` 从目录名取出目标名，目标不存在则以 `Target '<名字>' not valid.` 终止配置；为目标链接 `joyengine_core_pre_define_macro`（与核心一致的 `WO_SHARED_LIB` / `WO_STATIC_LIB` 等宏），并追加到 `JE4_STATIC_LINKED_PKG_AND_MODULE_LIST`，最终私有链接进 `joyecs`。
* **prebuild**：遍历 `module/` 中名字匹配 `.je4module`、且含 `CMakeLists.txt` 的目录，生成 `src/jeecs_extern_library_module.hpp`——为每个模块声明 `jestatic_module_<名字>_entry(woort_Dylib*)` 与 `jestatic_module_<名字>_leave()`，并生成 `je_extern_lib_module_init_impl`：以 `{"jeecs_module_entry", "jeecs_module_leave"}` 两个符号构造函数表，`woort_dylib_fake("<名字>", ...)` 注册为伪造动态库。

模块源码里写的仍是 `jeecs_module_entry` / `jeecs_module_leave`：编辑器模板的 `CMakeLists.txt` 在静态构建（`BUILD_SHARED_LIBS=OFF`）时以 `-Djeecs_module_entry=jestatic_module_<名字>_entry` 等宏重命名，避免多个模块的入口符号冲突；动态构建时则链接引擎核心库并导出原名。

⚠️ 注意：根 CMake 只为模块目标附加链接方式宏，不提供引擎头文件路径。编辑器模板通过缓存变量 `JEMODULE_ENGINE_CORE_INCLUDE_PATH` 获取头文件目录（编辑器构建时传入 `<引擎目录>/builtin/include`，该目录由 prebuild 从 `include/` 拷贝而来）；把模块放进 `module/` 静态链接时，需要保证它的 `CMakeLists.txt` 能找到引擎头文件。

### 运行时的登记与装载

`external_modules` 子系统在 `external_woolang_packages` 之后启动：调用 `je_extern_lib_module_init_impl` 注册全部伪造库，句柄保存在 `static_module_lib_handles` 中，关闭时以 `WOORT_DYLIB_UNREF_AND_BURY` 逐个卸载。**启动阶段并不调用模块入口**——woort 注册伪造库时只会调用表中名为 `woort_lib_entry` 的函数（[包](package.md)即依赖这一点），而模块表中登记的是 `jeecs_module_entry`。

`je_module_load(name, path)`（脚本侧 `je::load_module`）调用 `woort_dylib_load(name, path, nullptr, false)`。woort 先按名字查询库注册表，命中（包括伪造库）即直接返回；否则依次在工作目录、可执行文件目录、原样路径与系统路径中查找动态库。拿到库后若存在 `jeecs_module_entry` 符号就调用它。`je_module_unload` 先调用 `jeecs_module_leave`，再卸载库。这正是静态链接的模块也必须经 `je::load_module` 触发的原因；也因为按名字优先，导出入口生成的 `load_module(名字, "<可执行文件目录>/modules/lib<名字>...")` 对静态链接的同名模块同样有效。

类型守卫 `type_unregister_guard` 以本二进制中的 `typeid` 记录经它注册的类型；`entry::module_entry` 在守卫中登记内置组件与基础类型后调用 `je_towoo_api_update()` 刷新脚本声明，`entry::module_leave` 则经守卫注销全部类型。

⚠️ 注意：

* 静态链接时，伪造库的函数表里只有入口与出口两个符号——prebuild 不会扫描模块中的 extern 声明（这一点与[包](package.md)不同），因此脚本不能以 `extern("<名字>", ...)` 直接绑定静态模块中的函数，应通过模块注册的组件与系统使用其能力。
* 编辑器每次构建项目模块都会按模板**重新生成** `loader.hpp` 与 `CMakeLists.txt`，对这两个文件的手动修改会被覆盖。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 模块目录扫描、目标名校验、链接进核心 | `CMakeLists.txt`、`src/CMakeLists.txt` |
| 静态模块登记表生成 | `script/prebuild/prebuild.wo`（`generate_je_static_module_decls`） |
| 启动期登记 / 关闭期卸载 | `src/jeecs_extern_library_module.cpp` |
| `je_module_load` / `je_module_unload` | `src/jeecs_startup.cpp`（C API 见 [doc/capi/module](../capi/module/je_module_load.md)） |
| 入口函数类型、类型守卫 | `include/jeecs/typing.hpp`（`module_entry_t` / `module_leave_t`、`type_unregister_guard`） |
| 内置类型登记与 towoo 刷新 | `include/jeecs/entry.hpp` |
| 模块模板 | `builtin/editor/Editor/Template/`（`CMakeLists.txt.template`、`main.cpp.template`、`loader.hpp.template`、`native_module_*.template`） |
| 编辑器侧的构建与热重载 | `builtin/editor/Editor/ModulePrepare.wo`、`builtin/editor/Editor/Project/NativeModule.wo` |
