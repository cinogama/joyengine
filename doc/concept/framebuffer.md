# 帧缓冲（Framebuffer）

本文档是[术语表](README.md)中「帧缓冲」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述帧缓冲的创建、让摄像机渲染进帧缓冲以及把渲染结果当作纹理使用；
* **第二部分·底层实现**——面向引擎维护者，描述帧缓冲资源的结构、引用计数，以及它在渲染系统与图形 API 抽象层中的流转。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 帧缓冲是什么

*帧缓冲*（Framebuffer）是图形 API 层面的*渲染目标*：一组尺寸相同的纹理（称为*附件*），绘制结果写进这些纹理而不是屏幕。[摄像机](camera.md)默认把画面输出到屏幕；给摄像机挂上 `Camera::RendToFramebuffer` 并指定帧缓冲后，它的渲染结果就写进这个帧缓冲。

帧缓冲的颜色附件是普通的 2D [纹理](texture.md)，因此“渲染进帧缓冲”的画面可以继续被其他实体采样、或交给 [GUI](gui.md) 显示——这是画中画、小地图、编辑器场景视口等效果的基础。

### 创建帧缓冲：颜色附件与深度附件

创建时指定像素尺寸、颜色附件格式列表，以及是否附带深度附件：

| 参数 | 说明 |
| --- | --- |
| 宽、高 | 像素尺寸，**创建后不可修改**；为 0 时创建失败 |
| 颜色附件格式列表 | 每项一个颜色附件，按顺序编号 0、1、2……；列表为空时创建失败 |
| 是否含深度附件 | 为真时额外创建一个深度附件，编号紧跟在颜色附件之后 |

颜色附件格式（Woolang：`graphic::framebuffer::color_format`）：`RGBA` 为四通道、`MONO` 为单通道；`FLOAT16` 是附加标志，需要与通道布局**组合**使用（如 `RGBA` 与 `FLOAT16` 相加），得到半精度浮点附件，用于存放超出 0~1 范围的数据。

* 作为摄像机渲染目标时，通常需要深度附件，否则深度测试无从进行——编辑器的场景视口与 2D 光照的 G-buffer 都带深度附件；只做全屏合成的缓冲可以不带（2D 光照的光照缓冲即如此）。
* 尺寸固定意味着**窗口或视图尺寸变化时要重新创建**帧缓冲，再赋给摄像机。编辑器的做法可以参考：视口尺寸连续稳定 30 帧后才重建，避免拖拽缩放窗口时每帧重新分配 GPU 资源。
* 创建失败时返回 `none`（C++ 为 `std::nullopt`），并输出错误日志。

### 让摄像机渲染进帧缓冲

给摄像机挂上 `Camera::RendToFramebuffer`，再把帧缓冲设置进去：

* 组件中**尚未设置**帧缓冲时，渲染系统**跳过这个摄像机**——什么也不画。因此可以先挂组件，等帧缓冲创建好再设置；
* 摄像机的投影宽高比、`Viewport` 换算出的像素矩形、[UI](ui.md) 的布局尺寸，都改为以**帧缓冲的尺寸**为准，与窗口大小无关；
* 渲染到帧缓冲的摄像机**不参与** UI 的鼠标命中测试（帧缓冲是离屏目标，窗口鼠标不在其空间中）；
* `RendToFramebuffer` 持有帧缓冲的一份引用，只要组件还引用着它，帧缓冲就不会被释放。

⚠️ 注意 引擎每帧只会自动清除**屏幕**，不会自动清除帧缓冲：摄像机挂 `Camera::Clear` 时，也只清除**颜色附件 0**（深度附件则每次渲染前都会被清为 1.0）。不挂 `Clear`，上一帧的画面会留在帧缓冲里；有多个颜色附件时，附件 1 及之后的内容不会被摄像机清除（2D 光照管线对自己的 G-buffer 另行清除）。

### 把渲染结果当作纹理使用

`attachment(i)`（C++：`get_attachment(i)`）取出第 `i` 个附件，返回的就是一张 `texture2d`：

* 绑定到实体的纹理通道（`Renderer::Textures2D`），实体表面就显示这个摄像机的画面；
* 交给 GUI 的图片控件显示——编辑器场景视口就是把调试摄像机帧缓冲的附件 0 画进视口窗口；
* 在着色器中当作普通纹理采样——2D 光照管线的光照与合成 pass 正是这样读取 G-buffer 各附件的。

宇宙内（跨世界）的全部渲染分支按优先级升序提交，摄像机的优先级就是它的 `rend_queue`。想在**同一帧**内看到最新的画面，渲染进帧缓冲的摄像机的 `rend_queue` 应**小于**显示它的摄像机；相等时先后不确定，较大时显示的是上一帧的内容。

⚠️ 注意 同一世界中的每个摄像机都会渲染**全部**待渲染实体，引擎没有按摄像机过滤实体的机制。显示帧缓冲画面的实体如果也被“渲染该帧缓冲的摄像机”绘制，就会在同一次绘制中采样正在写入的纹理，结果由图形 API 决定，引擎不做检查。可以把显示用的实体放进另一个[世界](world.md)——摄像机只渲染本世界的实体。

⚠️ 注意 附件纹理没有 CPU 侧的像素数据，不能用 `pix` 读写像素。

### 典型场景

* **编辑器场景视口**（引擎内真实用例）：调试摄像机带 `RendToFramebuffer`，帧缓冲为一个 `RGBA` 颜色附件 + 深度附件，尺寸跟随视口窗口；Gizmo 通过自定义绘制画进同一个帧缓冲。
* **2D 光照**（引擎内真实用例）：`Light2D::CameraPostPass` 的 G-buffer 与光照缓冲、`Light2D::ShadowBuffer` 的阴影缓冲都是帧缓冲，由管线自动创建并在尺寸变化时重建（见[光照](light.md)）。
* **画中画、小地图、监视器画面**：一个摄像机渲染进帧缓冲，场景中的某个实体绑定其颜色附件。
* **后处理**：除 2D 光照的 `CameraPostPass` 外，引擎没有通用的后处理组件；可以让摄像机渲染进帧缓冲，再用全屏四边形实体或[自定义绘制](render.md#自定义绘制)采样其附件完成后处理。

### API 速览

#### Woolang（`je::graphic::framebuffer`，`Camera::RendToFramebuffer` 见 `je::towoo`）

| 接口 | 说明 |
| --- | --- |
| `framebuffer::create(w, h, formats, contain_depth)` | 创建帧缓冲，`formats` 为 `array<int>`（`color_format` 转 `int`），返回 `option` |
| `fb->attachment(index)` | 取第 `index` 个附件（`option<texture2d>`），越界为 `none` |
| `fb->size()` | 像素尺寸 `(w, h)` |
| `RendToFramebuffer::get` / `set` | 读取 / 设置摄像机的渲染目标（`option<framebuffer>`；设为 `none` 即清除，摄像机随之被跳过） |
| `render_branch::new_command_buffer(option::value(fb), x, y, w, h)` | 自定义绘制时以帧缓冲为目标；`w`、`h` 为 0 表示整个帧缓冲 |

```woolang
// 给尚未设置目标的摄像机创建 512×512 的帧缓冲（RGBA + 深度）
UPDATE!
{
    system! bind_target(r2b: Camera::RendToFramebuffer)
    {
        if (r2b->get->is_none)
            r2b->set(graphic::framebuffer::create(
                512, 512, [graphic::framebuffer::color_format::RGBA as int], true));
    }
}

// 把帧缓冲的颜色附件 0 绑定到某个实体的纹理通道 0
func show_on(screen: entity, fb: graphic::framebuffer)
{
    screen->bind_texture(0, fb->attachment(0));
}
```

#### C++（`jeecs::graphic::framebuffer`，头文件 `jeecs/graphic.hpp`）

| 方法 / 成员 | 说明 |
| --- | --- |
| `framebuffer::create(w, h, formats, contain_depth)` | `formats` 为纹理格式枚举的 `std::vector`（如 `JE_GL_TEXTURE_FORMAT_RGBA`），返回 `std::optional<basic::resource<framebuffer>>` |
| `get_attachment(index)` | `std::optional<basic::resource<texture2d>>`，越界为 `std::nullopt` |
| `width()` / `height()` / `size()` | 像素尺寸（`size()` 返回 `math::ivec2`） |
| `Camera::RendToFramebuffer::framebuffer` | `basic::optional<basic::resource<graphic::framebuffer>>`（头文件 `jeecs/components.hpp`） |

```cpp
using namespace jeecs;

void render_camera_to_screen_entity(game_entity camera, game_entity screen)
{
    basic::resource<graphic::framebuffer> fb = graphic::framebuffer::create(
        512, 512, { JE_GL_TEXTURE_FORMAT_RGBA }, true).value();

    camera.get_component<Camera::RendToFramebuffer>()->framebuffer.emplace(fb);
    screen.get_component<Renderer::Textures2D>()->bind_texture(
        0, fb->get_attachment(0).value());
}
```

### 相关术语

参见[术语表](README.md)：

* [摄像机](camera.md)——通过 `RendToFramebuffer` 把渲染结果写入帧缓冲；
* [渲染](render.md)——帧缓冲在渲染流程中的位置、自定义绘制；
* [纹理](texture.md)——帧缓冲的附件就是 2D 纹理；
* [光照](light.md)——2D 光照管线内部使用的 G-buffer 与阴影缓冲；
* [GUI](gui.md)——编辑器以图片控件显示帧缓冲附件。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。C ABI 结构与 C++ 封装见 `include/jeecs/graphic.hpp`；创建与释放实现于 `src/jeecs_graphic_api_basic.cpp`（`je_graphic_framebuf_create` / `je_graphic_framebuf_close`）；渲染系统中的使用见 `src/jeecs_core_graphic_system.cpp`；命令缓冲的录制与提交见 `src/jeecs_graphic_cmd.cpp`；Woolang 绑定见 `src/jeecs_graphic_woolang_api.cpp`。

### C ABI 结构与创建

```
je_gl_frame_buffer_t
 ├─ m_handle               资源句柄（偏移 0）：路径（帧缓冲恒为空）、引用计数、所属图形线程、m_modified、后端对象指针
 ├─ m_output_attachments   不透明的附件数组，实际是 basic::resource<graphic::texture2d>[]
 ├─ m_attachment_count     颜色附件数 + (含深度 ? 1 : 0)
 └─ m_width / m_height
```

`je_graphic_framebuf_create` 先校验尺寸与颜色附件数（任一为 0 即记错误日志并返回空指针），再 `malloc` 结构体与附件数组，用 placement new 逐个构造附件：颜色附件为 `texture2d::create(w, h, 格式 | JE_GL_TEXTURE_FORMAT_FRAMEBUF)`，深度附件为 `JE_GL_TEXTURE_FORMAT_DEPTH | JE_GL_TEXTURE_FORMAT_FRAMEBUF`，排在数组末尾。`FRAMEBUF` 与 `DEPTH`、`FLOAT16` 同属 `JE_GL_TEXTURE_FORMAT_NON_PIXEL_FORMAT_MASK`，带这些标志的纹理不分配 CPU 像素缓冲（`m_pixels` 为空）——这就是附件不能用 `pix` 访问的原因。最后 `init_resource_handle` 把引用计数初始化为 1。

此时没有任何图形 API 对象被创建：与其他图形资源一样，后端对象在首次使用时由图形线程惰性初始化。

### 两级引用计数

* **C 层**：`m_handle.m_raw_ref_count`。`jegl_share_resource`（即 `je_graphic_resource_handle_share`）加一，`je_graphic_framebuf_close` 减一；归零时先析构全部附件的 `resource<texture2d>`（各附件纹理的引用随之减少），释放附件数组，再把帧缓冲排入待销毁队列，由图形线程调用后端的 `framebuffer_close`。
* **C++ 层**：`graphic::framebuffer` 继承 `resource_basic<je_gl_frame_buffer_t>`，析构时调用一次 `je_graphic_framebuf_close`；对外以 `basic::resource<framebuffer>`（引用计数的 `shared_pointer`）共享这个封装对象。`RendToFramebuffer::framebuffer`、Woolang 的 `framebuffer` 句柄（GC 句柄内持有一份 `basic::resource`）都是这一层的引用。

命令缓冲在 `begin_pass` 时对目标帧缓冲 `retain`（C 层引用），直到 `reset` / `close` 才释放：即使脚本在录制之后立刻替换或丢弃帧缓冲，已录制的命令仍然可以安全提交。附件纹理是独立计数的 `texture2d`，被实体或命令缓冲引用时，即使帧缓冲已经释放，纹理对象本身仍然存活。

### 渲染系统中的使用

`GraphicPipelineBase::resolve_camera_target` 把摄像机解析为 `camera_target`：`RendToFramebuffer` 存在但 `framebuffer` 为空时返回 `nullopt`（本帧跳过）；否则取帧缓冲（或窗口）及其宽高。`update_camera_matrices` 同样以帧缓冲尺寸计算投影宽高比，UI 管线以它像素化元素矩形，`UpdateMouseOn` 对 `framebuffer != nullptr` 的目标直接返回。

`begin_camera_pass` → `new_rend_chain_for_camera` 把 `Viewport` 换算到帧缓冲像素尺寸，经 `je_graphic_branch_new_command_buffer(branch, fb->resource(), &viewport)` 从渲染分支取得命令缓冲；分支在 `reset` 之后立即录制 `begin_pass(target, viewport)`，随后录制清除、绑定与绘制。

2D 光照管线在 `GraphicUpdate` 中比较所需尺寸（G-buffer 跟随摄像机目标，阴影缓冲跟随窗口，均乘以各自的分辨率比例），按需 `framebuffer::create` 重建 `CameraPostPass` 与 `ShadowBuffer` 的帧缓冲（新对象赋值给组件的 `basic::optional`，旧对象随最后一份引用释放），绘制时以 `get_attachment(i)` 取出附件绑定到纹理通道。

### 图形线程上的初始化与目标切换

提交命令缓冲时（`je_graphic_cmd_commit`），遇到 `BEGIN_PASS`：

1. 目标非空则调用 `prepare_framebuffer_resource`：资源状态为 `NEED_INIT` 时调用后端 `framebuffer_init`，`NEED_UPDATE`（句柄 `m_modified`）时调用 `framebuffer_update`，`READY` 则什么也不做。后端在 `framebuffer_init` 中负责确保附件纹理已初始化，并创建自己的帧缓冲对象——例如 OpenGL 后端为每个附件 `glFramebufferTexture2D`，按颜色附件数设置 `glDrawBuffers`，深度附件挂到 `GL_DEPTH_ATTACHMENT`；
2. 调用后端 `begin_pass(target, viewport)`：目标为空指针时绑定屏幕（后端的主渲染目标），否则绑定该帧缓冲，然后设置视口；引擎同时重置本 pass 的绑定去重缓存。

引擎**没有“恢复渲染目标”这一步**：每个命令缓冲都以 `BEGIN_PASS` 显式指定自己的目标，提交时若 pass 未显式结束则自动补上 `END_PASS`，因此一个摄像机渲染进帧缓冲不会把目标状态泄漏给下一个命令缓冲。后端对清除的实现是整附件清除（OpenGL 在 `begin_pass` 中关闭了 scissor 后用 `glClearBufferfv`；DX11 用 `ClearRenderTargetView` / `ClearDepthStencilView`，越界的附件编号被忽略）。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| `je_gl_frame_buffer_t`、纹理格式标志、后端虚表的帧缓冲接口 | `include/jeecs/graphic.hpp` |
| `graphic::framebuffer` 封装、`resource_basic` | `include/jeecs/graphic.hpp`（`jeecs::graphic` 命名空间） |
| 创建、释放、图形线程侧准备 | `src/jeecs_graphic_api_basic.cpp`（`je_graphic_framebuf_create` / `je_graphic_framebuf_close` / `prepare_framebuffer_resource`） |
| 命令缓冲的目标与提交 | `src/jeecs_graphic_cmd.cpp`（`je_graphic_cmd_begin_pass` / `je_graphic_cmd_commit`） |
| 摄像机目标解析与 pass | `src/jeecs_core_graphic_system.cpp`（`resolve_camera_target` / `new_rend_chain_for_camera`） |
| 2D 光照的 G-buffer / 光照缓冲 / 阴影缓冲 | `src/jeecs_core_graphic_defer_light2d_system.cpp` |
| 后端实现（以 OpenGL 为例） | `src/jeecs_graphic_api_impl_opengl3.cpp`（`framebuffer_init` / `gl_render_begin_pass`） |
| Woolang 封装与绑定 | `je/graphic.wo`、`je/towoo/towoo.wo`（`RendToFramebuffer::get/set`）、`src/jeecs_graphic_woolang_api.cpp` |
| 编辑器场景视口的实践参考 | `builtin/editor/Editor/WorldEditorViewport/CameraControl.wo`（`updateWalkCameraFramebuffer`）、`WindowLifecycle.wo` |
