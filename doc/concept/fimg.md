# 资源镜像（fimg）

本文档是[术语表](README.md)中「资源镜像」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述资源镜像的作用、资源路径的查找规则以及如何通过导出生成镜像；
* **第二部分·底层实现**——面向引擎维护者，描述镜像格式、读取与打包在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 资源镜像是什么

资源镜像（fimg）是引擎的*发布打包格式*：导出游戏时，项目资源、编译后的脚本与着色器缓存被打包进同一个目录下的一组文件——一份清单 `fimg.je4index` 加上若干分片 `fimg.0.je4image`、`fimg.1.je4image`……

运行时，引擎从镜像中读取这些资源，**就像它们是磁盘上的普通文件一样**：游戏代码用同一个资源路径打开文件，不需要关心它来自磁盘还是镜像。

### 为什么需要资源镜像

* **发布包更整洁**——成千上万个资源文件被收拢为少量分片；
* **只携带运行所需的产物**——脚本以编译后的二进制形式存放，需要预处理的资源只携带[缓存](cache.md)，源文件与缓存生成脚本都不随包；
* **读取方式不变**——开发期读松散文件、发布后读镜像，资源路径与加载代码完全相同。

### 资源路径的两个前缀

镜像只服务于带前缀的*资源路径*：

| 前缀 | 含义 | 真实文件的根目录 |
| --- | --- | --- |
| `@/` | 运行时资源（游戏与项目资源） | 运行时路径：编辑器中是项目目录，导出后是可执行文件所在目录 |
| `!/` | 引擎内部资源（内置着色器、内置脚本等） | 宿主路径：引擎可执行文件所在目录 |

不带前缀的路径（绝对路径、相对路径）按普通文件打开，**不会查找镜像**。

### 查找顺序：真实文件优先

打开 `@/...` 或 `!/...` 路径时：

1. 先把前缀替换为对应的根目录，尝试打开磁盘上的真实文件；
2. 真实文件不存在时，再以**原始路径字符串**为键，在*默认镜像*中查找。

因此真实文件总是优先于镜像中的同名条目：开发期没有镜像，所有资源都是松散文件；发布后资源都在镜像里；在导出目录中放置同名真实文件，即可覆盖镜像中的版本。

默认镜像在引擎启动时从可执行文件所在目录加载。目录下没有清单视为“没有镜像”——这是开发期的正常情况，不会报错；清单存在但已损坏时记录错误并拒绝加载。

镜像是**只读**的：引擎的写操作（例如生成缓存）总是写到磁盘上的真实文件。

⚠️ 注意 镜像查找不做任何路径规范化：键是打包时写入的原样字符串（如 `@/resource/hero.png`）。`@/resource//hero.png`、`@/resource\hero.png` 这类在磁盘上也许能打开的写法，在镜像中都找不到。资源路径请统一以 `@/` 或 `!/` 开头、使用 `/` 分隔。

### 生成资源镜像：导出项目

资源镜像由编辑器的 **文件 → 导出项目...** 生成。向导依次确认启动世界、分辨率与镜像分片大小，点击“开始打包”后在后台打包，进度与错误显示在“项目导出”窗口中。产物位于 `<项目>/.export/`（每次导出前整个目录会被删除重建）：

```
.export/
 ├── fimg.je4index + fimg.<N>.je4image   资源镜像（项目文件、着色器缓存、启动脚本）
 ├── modules/lib<模块名>.dll              RELEASE 编译的原生模块（真实文件）
 ├── <项目名>.exe + 引擎运行库             引擎运行时
 └── licenses/                            引擎使用的第三方许可证
```

镜像中各类文件的处理方式：

* 项目文件以 `@/<相对路径>` 为键打包，引擎内部着色器以 `!/builtin/shader/internal/<文件名>` 为键打包；
* `.wo` 脚本预编译为二进制后打包（编译失败时原样打包源码）；
* 着色器、图片（连同 `.je4texture` 属性文件）、动画、瓦片集、瓦片地图只打包生成的 `<源>.je4cache`，源文件不随包；
* 启动脚本 `@/builtin/main.wo` 由向导按设置生成（装载模块、注册脚本类型、设置分辨率与标题、加载启动世界）并打包，它就是导出游戏的[入口](entry.md)；
* 以 `.` 开头的隐藏文件与目录（`.export`、`.temp`、`.git` 等）、`.je4project`、`.dmp` 以及 `.je4module` 中的模块源码不打包；
* 动态库（`.dll`/`.so`/`.dylib`）以真实文件复制而不进镜像——操作系统加载动态库需要真实文件。

导出向导的“镜像分片大小”一页同时提供压缩设置（压缩设置保存在项目的 `.temp/export_settings.toml`，下次导出时恢复）：

| 设置 | 默认值 | 说明 |
| --- | --- | --- |
| 启用压缩 | 开启 | 关闭时所有文件原样存储 |
| 压缩等级 | 6 | 1~9 的 deflate 压缩等级 |
| 最小压缩大小 | 1 KB | 小于该大小的文件原样存储 |
| 跳过扩展名 | 内置列表 | 命中的文件原样存储，默认列表为 PNG、JPEG、ZIP、MP3、MP4 等本身已压缩的格式 |
| 分片大小 | 128 MB | 每个分片文件的容量 |

### API 速览

#### Woolang（`je::file`、`je::internal::fimg_writer`）

| 函数 | 说明 |
| --- | --- |
| `je::file::readall(path)` | 读取文件全部内容（`option<string>`），资源路径按上述规则查找真实文件与镜像 |
| `je::internal::fimg_writer::create(root, shard_size, level, min_size, skip)` | 在已存在的目录 `root` 下创建打包器（导出流程使用的内部接口），`shard_size` 传 0 时为 1 MiB |
| `packer->pack(file_path, archive_path)` | 把磁盘文件以键 `archive_path` 写入镜像，键重复时失败 |
| `packer->pack_buffer(buffer, len, archive_path)` | 把内存中的数据作为文件写入镜像 |
| `packer->finish()` | 关闭最后一个分片并写出清单，成功后镜像才可用 |
| `packer->abort()` | 释放打包器；无论 `finish` 成功与否都要调用 |
| `je::internal::fimg_writer::default_skip_extensions()` | 内置的“已压缩扩展名”列表 |

读取资源时无需感知镜像：

```woolang
import je;

// 真实文件优先，不存在时从默认镜像读取
match (je::file::readall("@/resource/config.toml"))
{
    value(text)? je::log(text);
    none? je::logerr("找不到 @/resource/config.toml");
}
```

打包器的最小用法（导出流程的简化版）：

```woolang
import je;
import je::internal;

let packer = je::internal::fimg_writer::create(
    out_dir, 0, 6, 64, je::internal::fimg_writer::default_skip_extensions());

if (!packer->pack(real_path, "@/resource/config.toml"))
    je::logerr("打包失败");
if (!packer->finish())
    je::logerr("写出清单失败");

packer->abort(); // 释放打包器
```

#### C++

C++ 侧没有镜像专用的封装类。所有资源加载类（如 `jeecs/graphic.hpp` 中的 `graphic::texture2d::load`、`graphic::shader::load`）与文件读取都遵循同一套资源路径规则，自动从默认镜像读取。

### 相关术语

参见[术语表](README.md)：

* [缓存](cache.md)——导出时只携带缓存，缓存经由同一套路径规则从镜像中读取；
* [编辑器](editor.md)——“导出项目”向导负责生成镜像；
* [入口](entry.md)——导出游戏的入口脚本 `@/builtin/main.wo` 存放在镜像中；
* [模块](module.md)——原生模块以动态库形式随包，不进镜像；
* [着色器](shader.md)——引擎内部着色器与项目着色器都以缓存形式打包。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。格式的权威规范见 [doc/fimg/fimg_format.md](../fimg/fimg_format.md)；清单解析、读取与打包位于 `src/jeecs_basic_file_fimg_archive.cpp` / `.hpp`（压缩依赖 miniz），`je_file_*` 与路径解析、默认镜像位于 `src/jeecs_basic_file_image.cpp`，底层文件操作与启动初始化位于 `src/jeecs_basic_file.cpp`；C ABI 见 `include/jeecs/file.hpp`。

### 格式要点（v3）

镜像目录由一份清单与 N 个分片组成。清单使用小端字节序、所有整数显式序列化：56 字节的头部（magic `"JE4FIMG\0"`、版本 3、保留 flags、`shard_size`、条目数、分片数、`total_stored_size`、条目区的 CRC-64）后跟条目区；每个条目记录路径、起始分片、分片内偏移、`stored_size`（分片中实际占用的字节数）、`size`（逻辑大小）与 codec（0 = 原样存储，1 = raw deflate）。打包端用 `std::map` 收集条目，因此条目按路径排序，相同输入产生确定性输出。

v3 相对 v2 的变化是为每个条目增加了独立的压缩信息。读取端只接受版本 3，版本不符时整个清单被拒绝，旧版镜像需要重新导出。加载清单时还会拒绝 CRC 不匹配、路径为空或含 NUL、未知 codec、尺寸不一致、引用不存在的分片、末尾有多余字节等情况，完整清单见格式规范。

### 分片与条目的关系

打包器顺序追加数据：当前分片写满 `shard_size` 字节才关闭并开启下一个，分片在首次写入数据时才创建，因此不会出现空分片，除最后一个分片外每个分片都恰好是 `shard_size` 字节。

条目的存储字节在相邻分片间**连续**，大文件可以跨越多个分片：把全部分片首尾相接看作一条字节流，条目就是其中从 `shard * shard_size + offset` 开始、长 `stored_size` 的一段。读取时按 `shard + (offset + 游标) / shard_size` 与对应的取余定位分片与分片内偏移；加载清单时会验证条目最后一个字节所在的分片编号小于分片数。

### 读取路径

`je_file_open(path)` 的流程：

1. `_resolve_path` 把 `@` 替换为运行时路径（运行时路径为空时退回宿主路径）、`!` 替换为宿主路径，拼接时处理重复或缺失的分隔符；
2. 经底层文件操作打开解析后的路径，成功即返回原生文件句柄；
3. 失败且原始路径以 `@` 或 `!` 开头时，在共享锁下拷贝一份默认镜像的 `shared_ptr`，以**未解析的原始路径**在 `unordered_map` 中精确查找，命中则构造 `fimg::File`，`je_file_size` 返回条目的逻辑大小。

`fimg::File` 的读取行为：

* store 条目直接按逻辑偏移定位分片读取；
* deflate 条目惰性创建解压器（`mz_inflateInit2(-MZ_DEFAULT_WINDOW_BITS)`，即无 zlib 头的 raw deflate），以 64 KiB 为单位从分片读入压缩数据；
* 向前 `seek` 只记录逻辑位置，下次读取时解压并丢弃到目标位置；向后 `seek` 重置解压器并从头解压，代价为 O(目标位置)；
* 每个句柄同一时刻最多持有一个分片的原生句柄，读取跨分片时切换；
* 分片截断或压缩数据损坏时记录错误并置失败标记，此后读取一律返回 0，不会返回未初始化的数据。

镜像对象以 `shared_ptr<const Archive>` 共享，每个打开的 `fimg::File` 都持有一份引用，因此 `je_fimg_default_archive_set` 替换或关闭默认镜像不会使已打开的句柄失效；默认镜像指针本身由 `shared_mutex` 保护（打开文件取共享锁，替换取独占锁）。

⚠️ 注意 压缩条目的向后 `seek` 等于从头重新解压。二进制缓存文档（见[缓存](cache.md#二进制缓存文档)）每次按键查找都会回跳到文档起点，而 `je4cache` 不在默认跳过列表中——体积较大且查找频繁的缓存若被压缩，反复回跳就会变成反复解压。有需要时可以把 `je4cache` 加入导出设置的跳过扩展名，让缓存原样存储。

### 平台与底层文件接口

清单与分片都经由 `jeecs::file::native` 中可替换的文件操作读取（`je_file_native_operator_register`），而不是直接调用 `fopen`。启动时 `jeecs::file::bootup` 把宿主路径与运行时路径都设为可执行文件所在目录，并调用 `je_fimg_default_archive_set` 从该目录加载默认镜像，关闭时再以 nullptr 卸载；编辑器打开项目后把运行时路径改为项目目录，但默认镜像不随之改变。

Android 平台借此接入 APK 资源：注册的文件操作把以 `#` 开头的路径交给 AAssetManager 打开，并以 `je_fimg_default_archive_set("#")` 让引擎从 assets 根目录读取清单与分片。

### 写入路径

`fimg::Writer` 由 `je_fimg_writer_create` 创建：`shard_size` 为 0 时取 1 MiB；`options` 为 nullptr 时使用等级 6、最小 64 字节与内置跳过列表；压缩等级小于 0 时取 6、大于 9 时截为 9。

* **codec 选择**（`select_codec`）：等级为 0、空文件、小于 `min_compress_size`、扩展名命中跳过列表时原样存储，其余使用 deflate；不做“压缩后反而更大就回退”的判断；
* **流式压缩**：`mz_deflateInit2` 以负窗口位数生成 raw deflate 流，压缩输出按 64 KiB 缓冲直接追加进分片，不缓冲整个文件；
* **源文件一致性**：`pack_file` 流式读取源文件，实际读到的字节数与打开时的大小不一致即判定“源文件在打包期间被修改”，writer 进入失败状态；
* **重复键**：重复的 `archive_path` 被拒绝并记录警告，保留第一次写入；
* **提交**：`finish` 关闭当前分片并写出清单，任何写入或关闭失败都使 writer 进入失败状态且不会产生可用的新清单；`abort` 只释放 writer，已写出的分片与清单留在磁盘上，由调用方清理输出目录。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 格式规范（v3） | `doc/fimg/fimg_format.md` |
| 清单解析、`fimg::File` 读取、`fimg::Writer` 打包 | `src/jeecs_basic_file_fimg_archive.cpp` / `.hpp` |
| `je_file_*`、路径解析、默认镜像 | `src/jeecs_basic_file_image.cpp` |
| 底层文件操作与启动时加载默认镜像 | `src/jeecs_basic_file.cpp` |
| C ABI 与行为注释 | `include/jeecs/file.hpp`（逐项说明见 [doc/capi/file](../capi/file/)） |
| Woolang 绑定 | `je/je.wo`（`je::file::readall`）、`je/internal.wo`（`fimg_writer`）、`src/jeecs_file_woolang_api.cpp` |
| 导出流程与打包规则 | `builtin/editor/Editor/ExportProject.wo` |
| 导出游戏的启动脚本模板 | `builtin/editor/Editor/Template/runtime_main.wo.template` |
| Android 镜像接入 | `android/app/src/main/cpp/main.cpp` |
