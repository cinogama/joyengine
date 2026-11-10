# 缓存（Cache）

本文档是[术语表](README.md)中「缓存」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述缓存的行为约定、哪些资源会走缓存以及相关用法；
* **第二部分·底层实现**——面向引擎维护者，描述缓存文件的结构、校验与写入协议在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 缓存是什么

缓存（Cache，扩展名 `.je4cache`）是资源的*预处理产物*：引擎第一次加载某个原始资源时，先把它转换成适合运行时直接读取的二进制形式并写到磁盘上，之后的加载直接读取缓存，省去解析与转换。

缓存文件与原始资源放在一起，文件名是在原始路径后追加 `.je4cache`：`hero.png` 的缓存是 `hero.png.je4cache`，`sprite.shader` 的缓存是 `sprite.shader.je4cache`。编辑器的资源浏览器默认隐藏这些文件。

### 为什么需要缓存

原始资源的格式是为“便于编辑”设计的，而不是为“便于加载”设计的：

* 图片需要解码成像素；
* 动画、瓦片集与瓦片地图以 TOML 文本保存，需要解析与换算（例如动画中以欧拉角记录的旋转要换算成四元数）；
* 着色器是 Woolang 脚本，需要执行脚本并生成各图形后端的着色器源码。

这些工作只需要在资源变化时做一次。缓存还有另一层意义：**导出的游戏只携带缓存**，原始资源与负责转换的脚本都不随包（见[资源镜像](fimg.md)）。

### 对使用者透明的行为

缓存的创建、校验与重建都在加载资源时自动完成，加载接口与“直接读原始文件”没有任何区别：

* **首次加载较慢**——缓存不存在，引擎先生成缓存再读取；
* **之后加载变快**——直接读取缓存；
* **原始资源改动后自动重建**——缓存记录了原始资源内容的校验值（CRC64），下次加载发现不一致，缓存即失效并重新生成；
* **引擎升级后自动重建**——缓存还记录了格式版本号，缓存格式变化后旧缓存全部失效。

失效重建时日志会出现 `Reloading ..., cache file outdated (...)` 字样，括号内说明失效原因：缓存管理器版本变化、缓存格式版本变化或原始文件变化。

**原始资源不存在时，缓存不做校验直接使用**——导出的游戏正是依靠这一点，在只有缓存的情况下正常加载资源。

### 哪些资源走缓存

| 原始资源 | 缓存内容 | 生成者 | 校验依据 |
| --- | --- | --- | --- |
| 图片（png、jpg、bmp、tga 等） | 颜色空间 + 解码后的 RGBA8 像素 | 内置生成脚本 `texture.wo` | 图片与可选的属性文件 `<图片>.je4texture` |
| 动画 `.je4animation`（TOML） | 动作、轨道、关键帧组成的二进制文档 | 内置生成脚本 `animation.wo` | 源文件 |
| 瓦片集 `.je4tileset`、瓦片地图 `.je4tilemap`（TOML） | 图集与地图的二进制文档 | 内置生成脚本 `tileset.wo` / `tilemap.wo` | 源文件 |
| 着色器 `.shader` | 各图形后端的着色器代码（HLSL、GLSL、GLSL ES、MSL、SPIR-V）及配置与反射信息 | 引擎内置的着色器生成器 | 源文件 |
| 内置生成脚本、编辑器主脚本 | 编译后的 Woolang 二进制 | 引擎启动流程 | 见[第二部分](#编辑器主脚本的缓存) |

**纹理只经由缓存加载**：运行时不会绕过缓存直接解码图片，图片的属性文件（如颜色空间 `color_space = "linear"`）也随像素一起写进纹理缓存。

### 失败的生成不会留下有效缓存

生成缓存可能失败（TOML 写错、图片无法解码等）。引擎的约定是：**只有完整写完的缓存才会生效**。生成失败时磁盘上可能残留一个半成品文件，但它永远无法通过校验；修好源文件后，下次加载会自动重新生成，不需要手工删除缓存。

### 导出时预生成缓存

导出的游戏不携带缓存生成脚本及其依赖的 Woolang 源码，无法在游戏内重建缓存——缓存一旦缺失或失效，对应资源就加载失败。因此编辑器导出项目时，会先在编辑器进程内为项目中全部图片、动画、瓦片集、瓦片地图与着色器（以及引擎内部着色器）生成缓存，再只把缓存打进资源镜像，源文件不随包。

### API 速览

#### Woolang（`je::cache`、`je::file::cache`）

| 函数 | 说明 |
| --- | --- |
| `je::cache::generate_texture_cache(path)` | 确保图片的纹理缓存可用（缺失或失效时生成），成功返回 `true` |
| `je::cache::generate_animation_cache(path)` | 同上，对应 `.je4animation` |
| `je::cache::generate_tileset_cache(path)` | 同上，对应 `.je4tileset` |
| `je::cache::generate_map_cache(path)` | 同上，对应 `.je4tilemap` |
| `je::cache::binary_writer` | 二进制缓存文档写入器，供内置生成脚本的 `generate` 函数使用 |
| `je::file::cache::saveall(path, content)` | 把任意内容保存为 `<path>.je4cache`（不绑定原始文件） |
| `je::file::cache::readall(path)` | 读取 `saveall` 保存的内容，不做原始文件校验（当前实现的已知问题见[第二部分](#加载与校验)） |

日常加载资源时不需要调用这些函数；预生成接口主要服务于导出流程：

```woolang
import je;
import je::cache;
import je::graphic;

// 加载纹理：缓存缺失或失效时自动生成，对调用方透明
let tex = je::graphic::texture2d::load(option::none, "@/resource/hero.png");

// 导出前预生成缓存（编辑器的导出流程即如此调用）
if (!je::cache::generate_texture_cache("@/resource/hero.png"))
    je::logerr("生成纹理缓存失败");
```

#### C++

C++ 侧没有缓存专用的封装类：资源加载类（头文件 `jeecs/graphic.hpp`）内部自动使用缓存；需要自定义缓存格式的原生代码直接使用缓存 C API（见第二部分）。

```cpp
using namespace jeecs;

// 缓存的生成、校验与重建全部在加载函数内部完成（context 为图形上下文）
auto texture = graphic::texture2d::load(context, "@/resource/hero.png");   // std::optional
auto shader = graphic::shader::load(context, "@/resource/sprite.shader");
auto anim = graphic::animation::load("@/resource/hero.je4animation");      // 失败时得到空动画
```

### 相关术语

参见[术语表](README.md)：

* [资源镜像](fimg.md)——导出时缓存被打包进镜像，运行时缓存经由同一套路径规则读取；
* [纹理](texture.md)、[着色器](shader.md)、[动画](animation.md)、[瓦片地图](tilemap.md)——依赖缓存加载的资源；
* [编辑器](editor.md)——导出项目时负责预生成缓存。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。缓存文件的创建与校验位于 `src/jeecs_basic_cache_mgr.cpp`，二进制缓存文档的读写位于 `src/jeecs_basic_cache_binary_document.cpp`，各类缓存的版本号集中在 `src/jeecs_basic_cache_version.hpp`，生成器调度位于 `src/jeecs_basic_internal_scripts.cpp`；C ABI 与行为注释见 `include/jeecs/cache.hpp`（逐项说明见 [doc/capi/file](../capi/file/)）。

### 缓存文件的结构

缓存文件路径恒为 `filepath + ".je4cache"`，内容是 16 字节的头部加负载：

```
偏移 0    uint32   缓存管理器主版本号 CACHE_MANAGER_MAIN_VERSION（写入期间为占位值 0）
偏移 4    uint32   格式版本号（调用方给出，如 SHADER_CACHE_VERSION、ANIMATION_CACHE_VERSION）
偏移 8    uint64   原始资源的 CRC64 校验值
偏移 16   ...      负载：原始字节流（着色器缓存）或一个二进制缓存文档（其余缓存）
```

所有字段按本机字节序直接写出，缓存由本机生成，不保证跨端序兼容。主版本号用于整体作废全部缓存，格式版本号用于作废某一类缓存；**主版本号不能取 0**，0 是“未完成”占位值，`je_cache_file_load` 会显式拒绝。

### 加载与校验

`je_cache_file_load(filepath, format_version, virtual_crc64)` 经 `je_file_open` 打开 `<filepath>.je4cache`——因此缓存同样遵循 `@/`、`!/` 路径解析，并能从[资源镜像](fimg.md)中读取。随后依次比较主版本号与格式版本号，再按 `virtual_crc64` 处理校验码：

| `virtual_crc64` | 行为 |
| --- | --- |
| `-1`（全 1） | 不做一致性校验，**也不读取**头部的校验码字段 |
| `0` | 现场读取 `filepath` 计算 CRC64；结果为 0（文件不存在或为空）时跳过校验 |
| 其他值 | 直接与该值比较（虚拟文件、多文件组合键、固定键） |

“源文件缺失即跳过校验”依赖 `je_file_crc64` 的约定：文件不存在或为空时原样返回种子。纹理缓存借此把图片与可选的 `.je4texture` 链式计算成一个键（`crc64(.je4texture, crc64(图片, 0))`），任一文件变化都会使缓存失效，两者都缺失时键为 0、跳过校验。由于每次加载都要完整读一遍原始文件计算 CRC64，缓存省下的是解析与转换，而不是源文件的读取。

⚠️ 注意 以 `-1` 打开时校验码字段不会被消费，返回句柄的读取位置停在偏移 8 而非负载起点 16。目前唯一的 `-1` 调用方是 `je::file::cache::readall`（`wojeapi_file_cache_read_all`），它从当前位置按文件总长读取，读到的内容开头会多出 8 字节校验码（`saveall` 写入的固定值 1）。编辑器的“最近打开项目”记录（`builtin/history.je4cache`）就经由这对接口读写。

### 创建与提交

`je_cache_file_create(filepath, format_version, usecrc64)`：

1. `usecrc64 == 0` 时计算 `filepath` 的 CRC64，结果为 0 则记录警告并返回 nullptr——**无法为空文件或不存在的文件创建缓存**（动画编辑器保存时总会写出 `version`/`actions` 键，正是为了保证源文件非空）；
2. 目标路径中的 `@` 替换为运行时路径、`!` 替换为宿主路径，以 `"wb"` 打开（旧缓存立即被截断），写入 `[0][format_version][crc64]` 头部。缓存总是写成真实文件，镜像是只读的；新写出的真实文件此后会优先于镜像中的同名缓存被读取。

`je_cache_file_write` 只是对 `fwrite` 的转发。`je_cache_file_close(file, finished)` 是提交点：`finished == true` 时 `fseek` 回文件头写入真正的主版本号再关闭；`false` 时直接关闭，主版本号保持 0。缓存的“原子性”不依赖临时文件加重命名，而依赖这次回写——任何中途失败（包括进程崩溃）都只会留下主版本号为 0、永远无法通过校验的文件。

⚠️ 注意 `je/cache.wo` 中 `binary_writer` 的注释称失败时“调用方会删除半成品缓存”，实际实现并不删除文件，只是不回写主版本号。

### 二进制缓存文档

除着色器缓存外，负载都是一个*二进制缓存文档*。每个值以 1 字节类型标记开头：

* 标量 `BOOL`（固定 1 字节）、`INT8`~`UINT64`、`FLOAT32`/`FLOAT64`——紧跟固定宽度的本机字节序数据；
* `STRING` / `BUFFER`——紧跟 `uint64` 长度与内容，`STRING` 不含结尾的 `'\0'`；
* `LIST`——若干值后以 `END` 结束；`DOCUMENT`——若干 `[STRING 键][值]` 后以 `END` 结束。

标记值本身是文件格式的一部分，只能追加新类型，不能重排。写入端（`je_cache_binary_append_*`）是纯追加协议：没有缓冲也没有回填，容器的嵌套完全由调用方配对 `append_document`/`append_list` 与 `append_end` 来保证。例如内置脚本的编译缓存就是 `DOCUMENT { "content": BUFFER(编译产物) } END`。

读取端的句柄只是 `(je_file_t*, 偏移)` 二元组，不拥有文件，每次读取都会先 `seek` 到自己的偏移，所以不能与对同一文件的顺序读取混用。`je_cache_binary_document_index` 从文档起点线性扫描：键不匹配时由 `_jeecs_cache_binary_next_value_offset` 按标记跳过整个值（嵌套容器按深度计数跳到配对的 `END`），同名键取第一个；`je_cache_binary_list_index` 同样是 O(下标) 的扫描，遍历整个列表应使用 list iterator。每一步都做越界检查，损坏的缓存以返回 false 表现，而不会越界读取。

### 生成器调度

引擎启动时（`jeecs::internal_script::bootup`）加载 `!/builtin/script/internal/cache/` 下的 `animation.wo`、`tilemap.wo`、`tileset.wo`、`texture.wo` 四个生成脚本；这些脚本自身的编译产物也以 `INTERNAL_SCRIPT_CACHE_VERSION` 缓存，缓存损坏时回退到源码编译。

`je_animation_cache_open_or_create` 等四个 `*_cache_open_or_create` 接口的流程一致：先 `je_cache_file_load`；失败且生成器可用时进入 `generate_cache`——创建缓存文件，新建一个 VM 调用脚本的 `generate(cache, source)`，以其返回值作为 `finished` 关闭缓存，最后**重新** `je_cache_file_load` 一次再返回，生成结果同样要经过完整校验。生成器不可用（导出的运行时）时缓存失效即返回 nullptr。

另外两条路径不经过生成脚本：

* **着色器**：`je_graphic_shader_load` 先尝试缓存，失败则执行着色器脚本生成各后端源码，并由 `_jegl_create_shader_cache` 用 `je_cache_file_write` 直接写出（不使用二进制文档）。源码编译需要构建时开启 `JE4_ENABLE_SHADER_WRAP_GENERATOR`，否则只能读取缓存（日志 `Shader generator has been disabled.`）；
* **纹理**：`_jegl_open_image_texture_cache` 只在图片本身存在时才调用 `je_image_texture_cache_open_or_create`；图片不存在时直接以键 0 读取已有缓存，不触发生成器，避免探测 `@/icon.png` 这类可选图片时刷出生成失败日志。

### 编辑器主脚本的缓存

`je_core_main_script_entry` 在 `@/builtin/main.wo` 无法加载时转而启动编辑器：先尝试 `@/builtin/editor/main.wo` 的缓存，失败才编译源码，并把编译产物写入 `!/builtin/editor/main.wo.je4cache`。该缓存以固定值 `EDITOR_SCRIPT_CACHE_VERSION` 同时作为格式版本与虚拟校验值，**不随编辑器源码内容变化而失效**；CMake 配置阶段执行的 `script/prebuild/prebuild.wo` 在把 `builtin/` 复制到构建目录的同时会删除这个缓存。

⚠️ 注意 直接改动构建目录中的编辑器脚本副本不会使该缓存失效，引擎会继续运行旧的编译产物；应修改仓库中的源文件并重新执行 CMake 配置，或手工删除 `builtin/editor/main.wo.je4cache`。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 缓存文件的创建、校验与提交 | `src/jeecs_basic_cache_mgr.cpp` |
| 二进制缓存文档的读写 | `src/jeecs_basic_cache_binary_document.cpp` |
| 主版本号与各类格式版本号 | `src/jeecs_basic_cache_version.hpp` |
| 生成器加载与 `*_cache_open_or_create` | `src/jeecs_basic_internal_scripts.cpp` |
| 缓存生成脚本（含各文档结构注释） | `builtin/script/internal/cache/*.wo` |
| 着色器缓存 / 纹理缓存的读写 | `src/jeecs_graphic_shader_wrapper.cpp`、`src/jeecs_graphic_api_basic.cpp` |
| 编辑器主脚本缓存 | `src/jeecs_startup.cpp`（`je_core_main_script_entry`） |
| Woolang 绑定 | `je/cache.wo`、`je/je.wo`（`je::file::cache`）、`src/jeecs_file_woolang_api.cpp` |
| C ABI 与行为注释 | `include/jeecs/cache.hpp`（逐项说明见 [doc/capi/file](../capi/file/)） |
| 导出时的缓存预生成 | `builtin/editor/Editor/ExportProject.wo`（`packScriptCaches`、`packShaders`） |
