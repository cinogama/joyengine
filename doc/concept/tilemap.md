# 瓦片地图（TileMap）

本文档是[术语表](README.md)中「瓦片地图」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述瓦片地图的构成与用法；
* **第二部分·底层实现**——面向引擎维护者，描述上述行为在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 瓦片地图是什么

瓦片地图（TileMap）是一种快速绘制 2D 场景的机制：把场景划分为等大的格子，每个格子引用一块预先切好的图片——*瓦片*（tile）。开发者在编辑器中像刷油漆一样绘制格子，引擎负责把整张地图合批渲染出来，并可以按格子的通行性自动生成[物理](physics.md)碰撞体。

挂在实体上的地图由*瓦片地图系统*（类型名 `Tilemap::TilemapSystem`）实例化，它和其它[系统](system.md)一样需要注册进[世界](world.md)。

### 两种文档：图集与地图

| 文档 | 扩展名 | 内容 |
| --- | --- | --- |
| *图集*（Tileset） | `.je4tileset` | 若干张源纹理，按瓦片边长切成网格，**每个格子就是一块瓦片**；瓦片的名称、通行性、自动拼接类型、法线与属性默认值；属性定义（schema） |
| *地图*（Tilemap） | `.je4tilemap` | 宽×高的格子网格、瓦片边长（像素，用于图集采样）、引用的图集列表、若干*图层*、逐格属性覆盖 |

* 瓦片 id 由图集分配，从 1 开始，**跨保存稳定**：源纹理扩大时新格子获得新 id，删除的 id 不会复用，地图按 id 引用瓦片因此不会错位；
* 瓦片默认可通行；“地形”不是独立的概念，而是自动拼接类型不为“普通”的瓦片；
* 图集源纹理还可以附带按通道绑定的附加纹理（例如法线图集），与主纹理共用同一瓦片网格。

### 在编辑器中绘制

1. 资源浏览器右键“新建→瓦片图集”，在瓦片地图编辑器的“图集设置”中添加源纹理；
2. 右键“新建→瓦片地图”（默认 32×32 格、32 像素、单图层），打开后引用图集并绘制；
3. 把 `.je4tilemap` 文件拖进场景视口：编辑器创建带完整变换组件与 `Tilemap::Map` 的根实体，并自动为世界挂上瓦片地图系统。

地图编辑器与图集编辑器是两个独立的窗口。编辑中**尚未保存的修改也会实时同步**到所有世界中的地图实例，保存时才写入文件。

### 地图实例：Tilemap::Map

`Tilemap::Map` 挂在地图根实体上，成员 `map_path` 是地图文件的运行时路径（`@/` 或 `!/` 前缀）。瓦片地图系统会为根实体自动创建并回收负责渲染的隐藏子实体：它们不显示在实体列表中、不随世界保存，加载后自动重建。

* **1 格 = 1 单位长度**；根实体原点位于格 (0, 0) 的左上角，地图向 +x 与 −y 方向展开；
* 移动、旋转根实体会带动整张地图；根实体的 `LocalScale` 就是整图的比例尺。

### 图层的叠放与外观

* *渲染队列*（`rend_queue`）决定同一张地图各图层的绘制先后（升序，后绘制的盖住先绘制的），新建图层默认为 10000 + 层下标，与场景实体常用的小队列值错开；
* *z 偏移*是图层相对根实体的世界 z，只在该层与写深度的场景物体之间参与遮挡判断；
* 每个图层可以指定一个着色器实例与若干附加纹理（*图层样式*）。**图层样式是图层唯一的着色器来源**，没有配置样式的图层不会回落到默认着色器；新建地图时编辑器为默认图层指定了 `TilemapUnlitBlend`；
* 图层还可以设置 2D 光照的自阴影与遮挡阴影（关闭 / 常开 / 自动）及其强度，遮挡轮廓由引擎按瓦片占据情况自动生成（见[光照](light.md)）。

### 瓦片值、自动图块与属性

格子里存放的是一个整数*瓦片值*：0 表示空格；正值表示按原样绘制某块瓦片；负值表示按该瓦片的*自动拼接类型*绘制——引擎根据相邻格子自动挑选合适的变体，相邻格子变化时无需改写网格数据。支持的自动拼接类型有 RPGMaker XP 式、RPGMaker VX 式四象限与 blob47。Woolang 侧用 `tile_value` / `auto_value` / `decode_value` 生成与解析瓦片值，C++ 侧用 `tilemap::decode_cell`。

*属性*是类型化的数据（布尔、字符串、整数、浮点、二/三/四维向量），用于给格子附加游戏逻辑信息。读取时沿*取值链*解析：**逐格覆盖 → 瓦片默认值 → 图集 schema 默认值**；类型与实际存储不符时读取失败，不做隐式转换。

### 墙体：自动生成碰撞体

在根实体上（与 `Tilemap::Map` 同一实体）添加 `Tilemap::Wall`，系统会把地图中**不可通行的格子**（任一图层不可通行即视为墙）贪心合并为矩形，为每个矩形生成一个静态的 `Physics2D::Collider::Box` 子实体：

| 成员（默认值） | 说明 |
| --- | --- |
| `layerid`（0） | 生成的碰撞体所属的物理层，写入各 `Rigidbody::layerid` |
| `block_empty`（false） | 严格模式：没有图块的空格与地图外缘一圈同样视为墙 |
| `restitution`（0）/ `friction`（0.2） | 墙体的弹性与摩擦系数，运行时修改即时生效 |

地图编辑只要改变了通行性，碰撞体就会自动重建；纯外观的修改不会触发重建。

⚠️ 注意 墙体只是普通的静态刚体：世界里同样需要物理系统、`Physics2D::Scene` 配置，并且墙体要落在某个参与碰撞的碰撞组中（见[物理](physics.md#物理场景与配置脚本)）。

### 运行时查询

#### Woolang（`je::tilemap`）

```woolang
import je::tilemap;

let map = je::tilemap::Tilemap::open("@/maps/level1.je4tilemap")->unwrap;

// 角色所在格（坐标相对地图根实体；tile_center 则反过来求格子中心）
let (cx, cy) = map->tile_at_world(local_x, local_y);

if (!map->walkable(cx, cy))
    std::println("被墙挡住了");

// 属性按取值链解析，无值或类型不符为 none
match (map->property_int(0, cx, cy, "damage"))
{
    value(d)? std::println(F"受到 {d} 点伤害");
    none?;
}

// 找出全部图层（-1）中标记为出生点的格子
for (let (x, y) : map->collect_cells(-1, "spawn", true))
    std::println(F"出生点: ({x}, {y})");
```

`Tilemap` / `Tileset` 值本身持有文档的一个引用，被回收时自动释放。`je::tilemap` 同时提供完整的文档编辑接口（图层、瓦片、图集、属性），编辑器正是基于它实现的。

#### C++（`jeecs::tilemap`，头文件 `jeecs/tilemap.hpp`）

C++ 封装是只读查询接口，文档写入仍由编辑器完成：

```cpp
using namespace jeecs;

if (auto map = tilemap::map_document::open("@/maps/level1.je4tilemap"))
{
    auto& doc = map.value();
    tilemap::map_size size = doc->size();              // 宽、高、瓦片像素
    bool can_pass = doc->walkable(3, 5);               // 全图层合并的通行性
    auto damage = doc->get_property<int32_t>(0, 3, 5, "damage");
    auto spawns = doc->find_cells<bool>(-1, "spawn", true);
    if (auto c = doc->tile(0, 3, 5))                   // 解码后的瓦片值
        debug::loginfo("tileset %d, tile %d", c->tileset_index, c->tile_id);
}
```

### 源文件与缓存

`.je4tilemap` 与 `.je4tileset` 都是 **TOML 文本**，由编辑器负责写出；引擎运行时读取的是同目录下的 `<源文件>.je4cache` [缓存](cache.md)。缓存缺失或源文件改动后，第一次加载会自动重新生成缓存（稍慢），之后都直接读取缓存。导出项目时只打包缓存，源文件不随包。这一切对使用者透明：在编辑器里编辑、保存即可。

### 相关术语

参见[术语表](README.md)：

* [物理](physics.md)——墙体碰撞体的模拟者；
* [光照](light.md)——图层的自阴影与遮挡阴影；
* [缓存](cache.md)——TOML 源与运行时缓存的关系；
* [纹理](texture.md)、[着色器](shader.md)——图集与图层样式的组成；
* [编辑器](editor.md)——地图与图集的创作环境。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。文档模型位于 `src/jeecs_core_tilemap_model.hpp`，瓦片地图系统与 C ABI 实现位于 `src/jeecs_core_tilemap_system.cpp` / `.hpp`，缓存生成器为 `builtin/script/internal/cache/tilemap.wo` 与 `tileset.wo`，Woolang 绑定见 `je/tilemap.wo` 与 `src/jeecs_tilemap_woolang_api.cpp`。

### TOML → je4cache 管线

1. **写出**：编辑器的 `TilemapEditor/Serialize.wo` 从引擎文档读取当前状态，构造 TOML 树并自行写盘，再调用 `je_tilemap_map_path_bind` / `je_tilemap_tileset_path_bind` 把路径登记给文档。**引擎既不写盘，也不生成缓存。**
2. **校验**：加载文档时 `load_map_from` 调用 `je_tilemap_cache_open_or_create`，后者先以 `je_cache_file_load(path, TILEMAP_CACHE_VERSION, 0)` 打开 `<path>.je4cache`，依次校验缓存管理器主版本、格式版本与源文件内容的 CRC64（源文件不存在时跳过 CRC 校验，导出包即走这条路径）。
3. **生成**：校验失败时 `generate_cache` 以 `je_cache_file_create` 写入文件头（主版本留空占位），在临时 VM 中调用生成器脚本的 `generate(cache, source)`——脚本用 `pkg::toml` 解析源文件，以 `je_cache_binary_*` 写出二进制文档；成功后 `je_cache_file_close(file, true)` 回写主版本使缓存生效，再重新加载。生成器脚本在引擎启动时作为内部脚本编译，本身也带缓存。

格式版本常量 `TILEMAP_CACHE_VERSION` / `TILESET_CACHE_VERSION` 位于 `src/jeecs_basic_cache_version.hpp`。

⚠️ 注意 缓存文档结构在三处保持一致：模型头文件的读取端（顶部注释即格式说明）、两个生成器脚本，以及编辑器的 TOML 写出端。改动结构时必须同步修改并提升格式版本，否则旧缓存会被错误解析。生成器脚本刻意不导入 `je::tilemap`，TOML 枚举名映射在生成器与 `je/tilemap.wo` 中各有一份。

### 缓存文档的内容

* **图集**：名称、`next_tile_id`；`sources` 列表（纹理路径、瓦片像素、格数、纹理尺寸、逐格瓦片 id、附加纹理）；`properties`（schema：名称、类型、默认值）；`tiles`（id、名称、通行性、自动拼接类型、法线、blob47 变体、属性默认值）。
* **地图**：宽、高、瓦片像素；`tilesets` 路径列表；`layers` 列表（名称、可见/锁定、z、渲染队列、自阴影/遮挡阴影模式与强度、行优先的 `grid` 缓冲、着色器路径、附加纹理、uniform 值）；`cell_properties` 逐格覆盖。

### 文档注册表与引用计数

文档由进程级注册表 `Tilemap::documents` 按路径共享。注册键经过规范化——解析 `@/`、`!/` 前缀、统一斜杠、Windows 下转小写——保证同一文件的不同写法映射到同一实例，避免编辑器两侧各持一份副本互相覆盖。

引用计数的来源是 open/create 的调用方、使用该地图的世界、引用该图集的地图；计数归零后注册表放弃持有，实例在没有其它持有者时销毁，未保存的修改随之丢弃，下次打开重新从磁盘读取。Woolang 文档句柄的终结器会在 GC 工作线程上调用 unload，因此计数操作与注册表都由互斥量保护；文档**内容**仍是共享可变数据，不在此串行化。任何经 C ABI 的修改都会自增文档版本号。

### TilemapSystem：从文档到渲染分片

系统只实现 `StateUpdate`（第 2 阶段），在脚本的 `UPDATE!` 与渲染之前完成数据构建。每帧：

1. 收集所有带 `Tilemap::Map`（可选 `Tilemap::Wall`）的根实体并解析文档；
2. 用当帧查询定位全部 `RenderPart` / `WallPart` 子实体，回收所属根实体已不存在的“孤儿”；
3. 逐根比对*版本戳*（地图版本与所引用图集的实例身份、版本合成）：未变化时只对齐着色器、比例尺与阴影组件；变化时重扫网格，按 **图层 × 32×32 分块 × 图集 × 源纹理** 计算所需分片，增删分片实体并重写顶点。

不同源纹理的顶点无法合批进同一缓冲，所以分片键包含源纹理。分片实体挂在根实体的 `Transform::Anchor` 下，`LocalPosition.z` 取图层 z，`Rendqueue` 取图层渲染队列，`LocalScale` 由系统显式从根实体同步（变换系统的缩放不沿父链复合）。顶点缓冲在创建时按分块容量一次分配，之后整体重写并收缩激活的索引数，与粒子系统同款。

每张源纹理被构建为**每层一块瓦片的纹理数组**，绑定在通道 0（附加纹理按各自通道绑定，图层样式的附加纹理占槽位 1 起），着色器以 `tex2darray` 采样，相邻瓦片渗色在结构上不可能发生。自动图块在构建顶点时由 `resolve_cell_quads` 解析邻接变体，编辑器预览使用同一实现。

⚠️ 注意 系统**不跨帧缓存任何实体句柄**：碎片整理与组件增删都会搬动实体，隔帧句柄的组件访问会静默返回空。跨帧簿记只存版本戳、掩码哈希等纯数据，分片与墙体每帧经查询重新定位。同理，自阴影/遮挡阴影组件的增删走命令缓冲，下一帧才生效，遮挡轮廓由 `sync_part_blockshadow` 在挂载完成后的帧写入。

### 墙体生成

墙体掩码沿用 `je_tilemap_is_walkable` 的语义；`block_empty` 模式下空格也计入，并把掩码向外扩一圈表示地图外缘。`merge_wall_rects` 把掩码贪心合并为矩形，每个矩形创建一个子实体（`Rigidbody` + `Collider::Box` + `Friction` + `Restitution` + `Editor::Invisible` + `WallPart`，没有刚体类型标记即为静态）。`WallPart` 记录矩形在地图局部空间的中心与尺寸，系统据此每帧按根实体缩放同步碰撞体尺寸，并同步 `layerid` 与材质。

版本戳变化时重扫掩码并计算哈希，**哈希不变则不重建**，避免纯视觉编辑引起物理世界抖动；哈希变化时先回收全部旧墙体再整体新建——墙体属于低频编辑的静态数据，整体重建比逐矩形差分简单可靠。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 组件（Map / Wall / WallPart / RenderPart） | `include/jeecs/components.hpp`（`namespace Tilemap`） |
| C ABI 与 C++ 只读封装 | `include/jeecs/tilemap.hpp`（逐函数说明见 [doc/capi/tilemap](../capi/tilemap/)） |
| 文档模型、缓存读取、注册表、自动图块解析 | `src/jeecs_core_tilemap_model.hpp` |
| TilemapSystem 与 C ABI 实现 | `src/jeecs_core_tilemap_system.cpp` / `.hpp` |
| 缓存生成器 | `builtin/script/internal/cache/tilemap.wo`、`tileset.wo`（加载见 `src/jeecs_basic_internal_scripts.cpp`） |
| 缓存校验 | `src/jeecs_basic_cache_mgr.cpp`、`src/jeecs_basic_cache_version.hpp` |
| Woolang 绑定 | `je/tilemap.wo`、`src/jeecs_tilemap_woolang_api.cpp` |
| 编辑器（绘制、TOML 写出、拖入实例化） | `builtin/editor/Editor/TilemapEditor/`、`builtin/editor/Editor/WorldEditorViewport/WindowLifecycle.wo` |
