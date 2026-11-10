# 包（Package）

本文档是[术语表](README.md)中「包」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述包是什么、引擎依赖哪些包、脚本如何使用包；
* **第二部分·底层实现**——面向引擎维护者，描述包的安装、锁定，以及原生实现如何被静态链接进引擎。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 包是什么

包（Package）是 Woolang 语言的依赖管理单位：一个 git 仓库，内含 Woolang 源码，有的还带有 C/C++ 编写的原生实现。包由包管理器 *baozi* 管理，**包的版本就是 git 提交哈希**，由锁文件固定。

引擎脚本、[编辑器](editor.md)与游戏[脚本](script.md)都建立在一组包之上：标准库 `std`、文件系统 `fsys`、`toml`、`regex`、`thread`、`async` 等。游戏项目本身不需要 baozi——项目脚本直接使用随引擎提供的这些包。

### 引擎仓库与包

引擎仓库根目录的 `baozi.toml` 声明了引擎依赖的全部包：

```toml
name = "joyengineecs"
no_compile = true

[woolang]
[dependence]
[dependence.std]
path = "pkg"
src = "3rd/pkg"

[dependence.worst]
path = "pkg"

# ……其余依赖……

[test_dependence]
```

* `no_compile = true`：本仓库自身不作为 Woolang 包编译，只借 baozi 安装依赖；
* `path = "pkg"`：包安装到 `pkg/<包名>/`（Woolang 源码，以及 baozi 预先编译好的原生库）；
* `src = "3rd/pkg"`：额外把包的完整仓库（含原生源码）复制到 `3rd/pkg/<包名>/`，供引擎从源码静态链接；
* `baozi.lock.toml` 把每个包——以及 `woolang`、`woort`、`woodyn` 工具链——锁定到具体的提交。

当前的依赖分为两类：

| 类别 | 包 |
| --- | --- |
| 带原生实现（声明了 `src = "3rd/pkg"`） | `std`、`fsys`、`filestream`、`buffer`、`toml`、`regex`、`thread`、`async`、`process`、`socket`、`platform`、`guid`、`image`、`list`、`math`、`singleton`、`dumpling`、`crashhandler` |
| 纯 Woolang | `any`、`concept`、`decorator`、`interface`、`iterator`、`makenew`、`node_editor`、`try`、`worst`、`woshader` |

**CMake 配置时会自动执行 `baozi install`**（失败则配置失败；可用 `-DJE4_INSTALL_PKG_BY_BAOZI_WHEN_BUILD=OFF` 跳过），随后运行 prebuild 脚本把 `pkg/` 拷贝进构建目录。`pkg/` 与 `3rd/pkg/` 都被 `.gitignore` 忽略，克隆仓库后由首次配置自动生成。

### 在脚本中使用包

```woolang
import pkg::std;
import pkg::fsys;

func main()
{
    // 包声明的命名空间是 fsys，而不是 pkg::fsys
    std::println(F"当前工作目录：{fsys::workpath()->to_string}");
}
main();
```

`pkg::` 只是导入路径前缀，来自依赖项的 `path = "pkg"`；导入之后使用的名字由包自己声明的命名空间决定（`std`、`fsys`……）。编辑器为项目生成的 Wooly 配置把引擎目录列为扫描根，因此在 VSCode 中编写项目脚本时，`import pkg::xxx` 同样能被解析。

### 包的原生部分

带原生实现的包，在脚本中以 `extern("lib<包名>", "函数名")` 声明函数，在 C++ 中以 woort 运行时接口实现它们；库被装载、卸载时分别调用 `woort_lib_entry` / `woort_lib_leave`。以 `guid` 包为例：

```woolang
// guid.wo
namespace guid
{
    public using GUID = string
    {
        extern("libguid", "generate_guid")
            public func gen()=> GUID;
    }
}
```

```cpp
// guid/main.cpp（节选）
#define WOODYN_IMPL
#include <woort.h>

static woort_Dylib* this_dylib = nullptr;

WOORT_API void woort_lib_entry(woort_Dylib* lib_instance) { this_dylib = lib_instance; }
WOORT_API void woort_lib_leave(void) { this_dylib = nullptr; }

WOORT_API woort_api generate_guid()
{
    char generate_result[48];
    // ……由时间戳、自增计数与随机数拼出 GUID 字符串……
    return woort_ret_string(generate_result);
}
```

为引擎新增依赖时，在 `baozi.toml` 中添加 `[dependence.<包名>]`（带原生实现且需要静态链接的，同时写上 `src = "3rd/pkg"`），然后重新执行 CMake 配置。

### 相关术语

参见[术语表](README.md)：

* [脚本](script.md)——包的使用者；`je` 引擎脚本库本身不是包，而是随引擎发布的脚本目录；
* [模块](module.md)——与包并列的原生扩展，二者由同一个 CMake 选项控制静态链接；
* [编辑器](editor.md)——大量依赖 `dumpling`、`toml`、`singleton` 等包；
* [着色器](shader.md)——`.shader` 脚本建立在 `woshader` 包之上。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。安装与静态链接位于根 `CMakeLists.txt`，prebuild 相关逻辑位于 `script/prebuild/prebuild.wo`（`copy_pkgs`、`generate_je_3rd_pkg_decls`，产物 `src/jeecs_extern_library_3rd_pkg.hpp`），启动期登记见 `src/jeecs_extern_library_3rd_pkg.cpp`。

### 安装与 prebuild

CMake 配置阶段依次执行两个外部命令，任一失败即终止配置：

1. 在仓库根目录执行 `baozi install`（Windows 下为 `baozi.bat`），按锁文件安装 `pkg/` 与 `3rd/pkg/`；
2. 在 `script/prebuild/` 下执行 `baozi run ./prebuild.wo --enable-halt-when-panic 1`，并按选项追加 `-static-link-pkgs`、`-gles-win32`、`-webgl`。

prebuild 本身是另一个 baozi 项目（使用旧式的 `package.json` 清单与独立的锁文件），它负责：拷贝 `pkg/` 到 `build/pkg`（带 `-static-link-pkgs` 时只拷贝 `.wo` 文件）、生成 libjoyecs / 包 / 模块三张函数登记表、复制许可证，以及把 `je/`、`builtin/`、`include/` 拷贝进构建目录。

### 静态链接

`JE4_STATIC_LINK_MODULE_AND_PKGS=ON`（默认值；Android 与 WebGL 构建强制开启）时：

* **CMake**：设置 `BAOZI_BUILD_BY_MGR=OFF`、`BAOZI_BUILD_SHARED=OFF`、`BAOZI_INCLUDE_PATH`（指向 `3rd/woolang`、`3rd/woort` 的头文件）等缓存变量后，遍历 `3rd/pkg/*` 中带 `CMakeLists.txt` 的目录逐个 `add_subdirectory`；跳过 `JE4_IGNORED_WOOLANG_PKG_LIST` 中的包；目标名必须等于包名，否则终止配置；以编译定义把 `woort_lib_entry` / `woort_lib_leave` 重命名为 `je_static_wo_pkg_<包名>_entry` / `_leave`，避免多个包的同名符号冲突；最后私有链接进 `joyecs`。
* **prebuild**：扫描 `3rd/pkg` 下全部 `.wo` 中的 `extern("库名", "函数名")`，为每个函数生成 `WOORT_API woort_api 函数名(void);` 声明；再扫描各包的 `.c` / `.cpp` 是否定义了 `woort_lib_entry` / `woort_lib_leave`，把重命名后的函数以**原名**登记进 `lib<包名>` 的函数表；最后为每个库名生成一张表，以 `woort_dylib_fake(库名, ...)` 注册。
* **运行时**：`external_woolang_packages` 子系统（先于 `external_modules` 启动）注册这些伪造库；脚本中的 `extern("libfsys", ...)` 按库名命中伪造库，直接绑定到链接进引擎的函数。woort 在注册伪造库时即调用表中的 `woort_lib_entry`，卸载时调用 `woort_lib_leave`，与装载、卸载真实动态库时的行为一致。

关闭静态链接时，CMake 不扫描 `3rd/pkg`（也不扫描 `module/`），prebuild 把 `pkg/` 原样拷贝（含预编译的原生库），运行时由 woort 优先在脚本所在目录查找并装载 `lib<包名>` 动态库。

⚠️ 注意：

* 包的 extern 库名必须是 `lib<包名>`：入口与出口函数登记在 `"lib" + 包名` 的表中，库名不一致会使二者落入不同的伪造库。
* 在静态链接模式下，带原生实现的包必须声明 `src = "3rd/pkg"`，否则 CMake 与 prebuild 都看不到它的源码，构建目录中也不会有它的动态库，脚本将无法解析其 extern。
* `JE4_IGNORED_WOOLANG_PKG_LIST` 只影响 CMake，prebuild 仍会为被忽略的包生成登记代码并因此引用其函数。WebGL 构建同时设置 `JE4_IGNORED_WOOLANG_PKG_LIST="process;socket"` 并给 prebuild 传 `-webgl`（跳过这两个包），两边必须保持一致，否则链接失败。

### `pkg::` 前缀的解析

`pkg::` 并不是语言层面的命名空间，而是导入路径：`import pkg::fsys` 解析为 `pkg/fsys/` 下的 `fsys.wo`。Woolang 依次在导入方所在目录、工作目录、可执行文件目录中查找，最后才查虚拟文件系统；引擎运行时命中的是 prebuild 拷贝到构建目录（即可执行文件目录）的 `pkg/`。编辑器把工作目录切换为项目目录后，这一路径依然经由可执行文件目录解析。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 依赖声明 / 版本锁定 | `baozi.toml`、`baozi.lock.toml` |
| `baozi install`、prebuild 调用、包的扫描与静态链接 | `CMakeLists.txt`、`src/CMakeLists.txt` |
| 包登记表生成、`pkg/` 拷贝 | `script/prebuild/prebuild.wo`（产物 `src/jeecs_extern_library_3rd_pkg.hpp`） |
| 启动期登记 / 关闭期卸载 | `src/jeecs_extern_library_3rd_pkg.cpp` |
| 子系统启动顺序 | `src/jeecs_basic_bootup.hpp` |
| WebGL / Android 的静态链接配置 | `webgl/CMakeLists.txt`、`android/app/src/main/cpp/CMakeLists.txt` |
