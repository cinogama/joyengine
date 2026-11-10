# 纹理（Texture）

本文档是[术语表](README.md)中「纹理」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述纹理的种类、加载与创建、像素读写以及如何绑定到实体；
* **第二部分·底层实现**——面向引擎维护者，描述上述行为在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 纹理是什么

纹理（Texture）是承载像素数据的图像*资源*。渲染时，[着色器](shader.md)按纹理坐标（UV）从纹理中*采样*颜色，把它贴到[网格](mesh.md)表面。纹理本身只是数据：它以什么过滤、寻址方式被采样，由着色器声明的采样器决定；它贴在哪个实体上，由实体的 `Renderer::Textures2D` 组件决定。

纹理是**引用计数的共享资源**：脚本中的纹理句柄、组件里的纹理引用各持有一份引用，最后一份引用释放时纹理才被销毁，对应的显存由图形线程在适当时机回收。

### 纹理的种类

| 种类 | Woolang 类型 | C++ 类型 | 说明 |
| --- | --- | --- | --- |
| 2D 纹理 | `texture2d` | `graphic::texture2d` | 最常用的贴图；唯一能从图片文件加载、能用像素句柄读写的种类 |
| 纹理数组 | `texture_array` | `graphic::texture_array` | 多张同尺寸 2D 图层打包为一个资源，按层号采样；瓦片地图用它存放图集 |
| 3D 纹理 | `texture3d` | `graphic::texture3d` | 宽 × 高 × 深的体积纹素数据 |
| 立方体贴图 | `texture_cube` | `graphic::texture_cube` | 固定 6 个同尺寸的正方形面 |
| MSAA 纹理 | `texture_msaa` | `graphic::texture_msaa` | 多重采样纹理，只存在于 GPU 侧，不持有 CPU 像素 |

除 2D 纹理外，其余种类只能程序化创建；Woolang 创建的各类纹理一律为 RGBA 格式。

### 加载与创建

#### 从图片文件加载

`texture2d::load(universe, path)` 从图片文件加载 2D 纹理，失败（文件不存在、无法解码、也没有可用缓存）时返回 `none`。路径遵循引擎的文件规则：`@/` 指向运行时目录（项目资源），`!/` 指向引擎可执行文件所在目录（内置资源位于 `!/builtin/`）。

* **图片会被预处理**：首次加载时图片被解码并写入*缓存* `<图片路径>.je4cache`，之后直接读取缓存；图片或其属性文件变化后缓存自动重建（见[缓存](cache.md)）。解码后的像素一律是 8 位 RGBA。
* **颜色空间**：图片默认按 sRGB 颜色贴图处理，采样时由硬件转换为线性值；法线图、遮罩等非颜色数据应在属性文件 `<图片路径>.je4texture`（TOML）中写 `color_space = "linear"`。
* **按路径共享**：传入宇宙（`option::value(universe)`）时，同一宇宙中**同一路径只会得到同一份纹理**——修改它的像素会影响所有使用者；传入 `option::none` 则每次得到独立的新实例。

#### 程序化创建

`texture2d::create(w, h)` 创建一张 RGBA 纹理，像素初始为全 0（透明黑）；宽或高为 0 时按 1 处理。C++ 侧可以指定像素格式，例如单通道的 `JE_GL_TEXTURE_FORMAT_MONO`；带有 `FLOAT16`、`DEPTH`、`FRAMEBUF` 标志的格式不保留 CPU 像素（帧缓冲附件即属此类）。

程序化创建的纹理没有资源路径，`path()` 返回 `none`。

#### 裁剪

`texture2d::clip(src, x, y, w, h)` 新建一张 w × h 的纹理（沿用源纹理的通道数与 sRGB 标志），并复制源纹理中以 (x, y) 为起点的矩形区域。结果与源纹理互相独立，也没有资源路径（当前实现的限制见[第二部分](#裁剪的实现限制)）。

### 像素读写

`texture2d::pix(tex, (x, y))` 取得一个像素句柄，`get_color` 读出 `(r, g, b, a)`（各分量 0~1），`set_color` 写入（钳制到 0~1 后量化为 8 位）。

* **坐标原点在左下角**：x 向右、y 向上，像素数据也按自下而上的行序存储；
* 越界坐标读到全 0，写入被忽略；单通道纹理读出 `(v, v, v, v)`，写入只取 r 分量；
* 写入只修改 CPU 侧的像素副本，并记录“已修改”；纹理下一次被图形线程使用时**自动重新上传**，无需手动提交；
* **不持有 CPU 像素的纹理（如帧缓冲附件）不能读写像素**。

纹理数组、3D 纹理与立方体贴图没有像素句柄：C++ 侧通过 `pixels()` / `layer_pixels(layer)` 直接改写像素缓冲后，调用 `mark_modified()` 触发整张纹理重新上传（Woolang 侧只提供纹理数组的 `mark_modified`）。

### 绑定到实体：纹理通道

实体通过 `Renderer::Textures2D` 组件持有纹理：组件内是若干 *纹理通道*（通道号 → 2D 纹理），以及 `tiling` / `offset` 两个 UV 变换参数（作为内置 uniform 传给着色器）。通道号与着色器中 `texture2d::uniform(通道号, 采样器)` 声明的通道一一对应。

* Woolang：`entity::bind_texture(e, 通道, option::value(tex))` 绑定或替换，传 `option::none` 解绑；`entity::get_textures(e)` 取出已绑定的通道表。**这些接口只在实体已挂有 `Renderer::Textures2D` 时生效**，否则静默无效果；
* 渲染时，引擎先在通道 0 绑定一张**紫灰相间的 2×2 棋盘格默认纹理**，再按组件内容覆盖——着色器采样通道 0 却看到棋盘格，说明没有绑定纹理；
* 实体的所有着色器 pass 共用同一组纹理绑定（见[着色器](shader.md)与[渲染](render.md)）。

**名称提示**：组件与接口中的 “pass”（如 `m_pass_id`）指的是**纹理通道**，与着色器列表中的“绘制 pass”不是一回事；编辑器的材质面板统一称之为“纹理通道”。

纹理数组通过平行的 `Renderer::Textures2DArray` 组件绑定，目前由 C++ 系统填写（瓦片地图分片把图集绑在通道 0），着色器侧用 `texture2darray` 采样；同一通道上数组纹理后绑定、覆盖 2D 纹理。

### 路径与场景保存

场景保存时，实体的纹理按路径记录，**没有路径的纹理不会被保存**；恢复时按路径重新加载（加载失败的通道保持原状）。

`texture2d::bind_path(tex, path)` 只改写纹理记录的路径：它不读写文件、不改动像素、也不登记进按路径共享的缓存。程序化生成的纹理绑定路径后即可被场景保存引用，但该路径必须指向真实存在的图片，否则重新加载场景时取不回这张纹理。

### API 速览

#### Woolang（`je::graphic`）

| 函数 | 说明 |
| --- | --- |
| `texture2d::load(universe, path)` | 从图片加载，失败返回 `none`；传入宇宙时按路径共享 |
| `texture2d::create(w, h)` | 创建 RGBA 纹理（像素全 0） |
| `texture2d::clip(src, x, y, w, h)` | 复制矩形区域为新纹理 |
| `texture2d::size(tex)` / `texture2d::path(tex)` | 宽高；资源路径（可能为 `none`） |
| `texture2d::bind_path(tex, path)` | 改写纹理记录的路径 |
| `texture2d::pix(tex, (x, y))` | 像素句柄，配合 `get_color` / `set_color` |
| `texture3d::create` / `texture_cube::create` / `texture_msaa::create` | 创建其它种类纹理 |
| `texture_array::create(w, h, layers)` | 创建纹理数组；另有 `layers`、`mark_modified` |
| `entity::bind_texture(e, 通道, tex)` / `entity::get_textures(e)` | 实体纹理通道的绑定与查询 |

```woolang
import pkg::std;
import je;
import je::ecs;
import je::graphic;

using je;
using je::graphic;

func setup_texture(e: entity, univ: universe)
{
    match (texture2d::load(option::value(univ), "@/texture/player.png"))
    {
        value(tex)?
            e->bind_texture(0, option::value(tex)); // 实体须已挂 Renderer::Textures2D
        none?
            je::logerr("纹理加载失败");
    }

    // 程序化创建 2x2 纹理并逐像素写入；坐标原点在左下角
    let checker = texture2d::create(2, 2);
    checker->pix((0, 0))->set_color((1., 1., 1., 1.));
    let (r, g, b, a) = checker->pix((0, 1))->get_color;
    std::println(F"({r}, {g}, {b}, {a})");
}
```

#### C++（`jeecs::graphic`，头文件 `jeecs/graphic.hpp`）

| 方法 | 说明 |
| --- | --- |
| `texture2d::load(context, path)` | 返回 `std::optional`；context 为 nullptr 时不参与按路径共享 |
| `texture2d::create(w, h, format)` | 指定像素格式创建 |
| `texture2d::clip(src, x, y, w, h)` | 同 Woolang |
| `tex->pix(x, y).get()` / `.set(vec4)` | 读写像素 |
| `texture3d` / `texture_array` / `texture_cube` / `texture_msaa` 的 `create(...)` | 其它种类，需显式给出格式 |
| `pixels()` / `layer_pixels(layer)` + `mark_modified()` | 直接改写 CPU 像素后触发重新上传 |
| `Renderer::Textures2D::bind_texture` / `remove_texture` / `get_texture` | 实体纹理通道（头文件 `jeecs/components.hpp`） |

```cpp
using namespace jeecs;

auto tex = graphic::texture2d::load(nullptr, "@/texture/player.png");

basic::resource<graphic::texture2d> checker =
    graphic::texture2d::create(2, 2, JE_GL_TEXTURE_FORMAT_RGBA);
checker->pix(0, 0).set({ 1.f, 1.f, 1.f, 1.f });

if (auto* textures = entity.get_component<Renderer::Textures2D>())
    textures->bind_texture(0, checker);
```

### 相关术语

参见[术语表](README.md)：

* [着色器](shader.md)——声明纹理通道与采样器，决定纹理如何被采样；
* [网格](mesh.md)——提供纹理坐标（UV）；
* [渲染](render.md)——渲染系统为每个实体绑定纹理并绘制；
* [帧缓冲](framebuffer.md)——渲染目标的附件本身就是 2D 纹理；
* [缓存](cache.md)——图片纹理缓存 `.je4cache` 的生成与失效；
* [瓦片地图](tilemap.md)——纹理数组的主要使用者。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。纹理数据结构与 C ABI 定义在 `include/jeecs/graphic.hpp`，创建与加载实现位于 `src/jeecs_graphic_api_basic.cpp`，Woolang 绑定见 `je/graphic.wo` 与 `src/jeecs_graphic_woolang_api.cpp`，图片缓存生成器为 `builtin/script/internal/cache/texture.wo`。

### 纹理数据结构与像素格式

五种纹理都继承公共前缀 `je_gl_texture_common_t`：资源句柄 `m_handle`（`static_assert` 保证位于偏移 0，以复用统一的资源生命周期管理）、类别标签 `m_kind`（`je_gl_texture_kind_t`，供 Woolang 包装层等类型被擦除的边界区分类别）、`m_pixels`、`m_width` / `m_height`、`m_format`，以及脏矩形 `m_modified_min/max_x/y`。派生类型各自追加 `m_depth`（3D）、`m_layers`（数组）、`m_samples`（MSAA）；立方体贴图按 6 × 宽 × 高排布，面序与图形 API 的 +X..−Z 一致。

`je_gl_texture_format_t` 的低 4 位是通道数，恰好等于每像素字节数（`MONO = 1`、`RGBA = 4`，掩码 `COLOR_DEPTH_MASK`）；其余位是标志：`FLOAT16`、`DEPTH`、`FRAMEBUF`、`SRGB`（采样时硬件解码 sRGB，存储不变，alpha 恒为线性）。前三者合称 `NON_PIXEL_FORMAT_MASK`，带有其一的纹理不分配 CPU 像素（`m_pixels == nullptr`）；MSAA 纹理在创建时被强制加上 `FRAMEBUF`。各 `create` 接口把为 0 的尺寸按 1 处理，并用 0 填充像素缓冲。

### 图片纹理的加载链路

`je_graphic_texture_2d_load(context, path)`：

1. context 非空时先查共享资源缓存（`_jegl_try_load_shared_resource`），命中则增加引用计数并返回同一实例；
2. 打开图片缓存：图片文件存在时调用 `je_image_texture_cache_open_or_create`——校验码是图片与 `.je4texture` 内容连续计算的 CRC64，缺失或失效时运行生成器脚本重建；图片不存在时直接读取现有缓存（导出的运行时只携带缓存，也避免 `@/icon.png` 这类可选探测刷出生成失败日志）；
3. 解析缓存文档的 `width` / `height` / `color_space` / `pixels`（RGBA8，自底向上，宽 × 高 × 4 字节）；缓存存在却无法解析视为损坏并记录错误，单纯缺失只记录 info；
4. 以 `RGBA` 格式建立纹理，颜色空间为 sRGB 时追加 `SRGB` 标志，最后经 `_jegl_try_update_shared_resource` 登记进共享缓存（若同一瞬间已有同路径资源登记，丢弃新建的实例，返回已缓存的那份）。

生成器 `texture.wo` 用 `pkg::image` 解码图片、用 `pkg::toml` 读取颜色空间，按 `je_cache_binary_*` 文档格式写出。编辑器的热重载走 `je_graphic_shared_resources_mark_outdated`：把路径移出共享缓存并标记其 GPU blob 过时，之后的加载会重新读取，已加载的实例不受影响。

### 上传与修改：m_modified 与脏矩形

图形线程使用资源前检查其状态（`_jegl_check_resource_state`）：首次在该图形线程（或线程重启后的新版本）中使用为 NEED_INIT，调用后端的 `texture_*_create_blob` + `texture_*_init`；`m_handle.m_modified` 为真时为 NEED_UPDATE，调用 `texture_*_update` 并清除标记；否则为 READY。资源与首个使用它的图形线程绑定，跨图形线程使用会报错。

`texture2d::pixel::set` 在首次写入时把脏矩形初始化为该像素并置 `m_modified`，之后的写入扩展矩形；`mark_modified`（`detail::mark_texture_modified`）则在未标记时把整张纹理标为脏。后端可以只上传脏矩形（Vulkan 后端对 2D 纹理如此），OpenGL 后端则整张重传。各后端经 `je_gl_graphic_api_t` 函数表实现每类纹理的 create_blob / init / update / close，例如 OpenGL 后端为带 `SRGB` 标志的 RGBA 纹理选用 `GL_SRGB8_ALPHA8` 内部格式。

### 资源与组件槽位

Woolang 的 `texture2d` 是包装 `basic::resource<graphic::texture2d>` 的 gchandle，后者是指向 RAII 包装 `resource_basic` 的共享指针，析构时调用 `je_graphic_texture_2d_close` 减少引用计数。所有 `resource<textureN>` 的布局一致，`woje_texture_kind_of` 借此按 2D 视图读出 `m_kind`：实体绑定（`woje_bind_texture_to_component`）据此拒绝非 2D 纹理，命令缓冲的 `bind_texture` 据此分派到 2D / 3D / 数组 / 立方体的绑定接口（MSAA 不受支持，会报错）。

`Renderer::Textures2D::textures` 是 `texture_with_passid` 的列表，每项持有一份纹理引用；`bind_texture` 替换同通道的项或追加新项。组件只反射了 `tiling` / `offset`，纹理列表不走通用的成员编辑，编辑器材质面板因此经实体接口就地修改。渲染时 `GraphicPipelineBase::bind_entity_textures` 依次绑定默认纹理（通道 0）、`Textures2D` 各项、`Textures2DArray` 各项，并返回 tiling / offset 供 `JE_UV_TILING` / `JE_UV_OFFSET` 使用。默认纹理由 `DefaultResources` 创建：(0,0)、(1,1) 为 (1, 0.25, 1)，(0,1)、(1,0) 为 (0.25, 0.25, 0.25)。

### 裁剪的实现

`texture2d::clip` 按源纹理宽度计算行跨度（`src_pixels + (x + (y + iy) * 源宽度) * color_depth`），逐行 `memcpy` 到新纹理；裁剪矩形先与源纹理边界求交（`copy_w = min(w, 源宽 − x)`、`copy_h = min(h, 源高 − y)`），越出源纹理的区域不拷贝、目标保持新纹理的初始内容。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 纹理结构、像素格式、C ABI 与行为注释 | `include/jeecs/graphic.hpp` |
| C++ 包装（`texture2d` / `pixel` / `clip` / `mark_modified` 等） | 同上（`jeecs::graphic` 命名空间） |
| 创建、图片加载、共享缓存与资源状态机 | `src/jeecs_graphic_api_basic.cpp` |
| 图片纹理缓存生成器 | `builtin/script/internal/cache/texture.wo` |
| Woolang 绑定 | `je/graphic.wo`、`je/ecs.wo`、`src/jeecs_graphic_woolang_api.cpp` |
| `Renderer::Textures2D` / `Textures2DArray` 组件 | `include/jeecs/components.hpp` |
| 默认纹理与实体纹理绑定 | `src/jeecs_core_graphic_system.cpp`（`DefaultResources`、`bind_entity_textures`） |
| 后端实现参考 | `src/jeecs_graphic_api_impl_opengl3.cpp`、`src/jeecs_graphic_api_impl_vk120.cpp` |
