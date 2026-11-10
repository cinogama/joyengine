# 渲染（Renderer）

本文档是[术语表](README.md)中「渲染」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述实体如何参与渲染、渲染顺序与渲染管线的选择；
* **第二部分·底层实现**——面向引擎维护者，描述渲染系统的工作流、绘制命令的录制与提交时序，以及图形 API 抽象层与各后端。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 渲染是什么

渲染（Renderer）是把世界中满足特定切片要求的组件集合，呈递为人眼可见画面的过程。执行渲染的系统称为*渲染系统*，也称*渲染管线*——在本文档中，这两个词**仅特指**引擎内负责渲染的系统，而不是 GPU 层面“可编程渲染管线”的含义。

渲染系统就是普通的[系统](system.md)：它被注册进[世界](world.md)，每帧通过[切片](slice.md)收集本世界的[摄像机](camera.md)与待渲染实体，然后**为每个摄像机渲染一次全部待渲染实体**。由此可知：

* 世界里没有装入渲染系统，这个世界的内容就不会出现在画面上；
* 世界被停用时，渲染系统随其他系统一起停止，并释放它占用的渲染分支——该世界的内容随之从画面上消失。

### 内置的三条渲染管线

引擎内置三个渲染系统，以类型名注册，像其他系统一样添加到世界中即可工作：

| 系统类型名 | 职责 |
| --- | --- |
| `Graphic::UnlitGraphicPipelineSystem` | 无光照的前向渲染，绘制世界空间中的实体；编辑器新建世界时默认装入 |
| `Graphic::DeferLight2DGraphicPipelineSystem` | 2D 延迟光照：在相同的实体集合上增加 2D 光源、阴影与摄像机后处理合成（见[光照](light.md)） |
| `Graphic::UserInterfaceGraphicPipelineSystem` | 绘制 UI 元素（四边形背景与逐字文本，见 [UI](ui.md)） |

Unlit 与 DeferLight2D 用完全相同的切片收集世界空间实体，**一个世界通常只装其中之一**，否则同一批实体会被绘制两次；UI 管线只处理带 UI 元素组件的实体，与前两者负责的实体互不重叠。

### 实体参与渲染需要哪些组件

世界空间管线（Unlit / DeferLight2D）按下表收集待渲染实体：

| 组件 | 要求 | 作用 |
| --- | --- | --- |
| `Transform::Translation` | 必须 | 实体的世界变换，提供模型矩阵（见[变换](transform.md)） |
| `Renderer::Shape` | 必须 | 网格（见[网格](mesh.md)）；未指定网格时使用引擎内置的单位四边形 |
| `Renderer::Shaders` | 必须 | 着色器列表（见[着色器](shader.md)）；列表为空时使用引擎默认着色器 |
| `Renderer::Textures2D` | 可选 | 按纹理通道绑定的 2D 纹理，以及 UV 平铺 `tiling` / 偏移 `offset`（见[纹理](texture.md)） |
| `Renderer::Textures2DArray` | 可选 | 按纹理通道绑定的纹理数组，[瓦片地图](tilemap.md)分片用它绑定图集 |
| `Renderer::Rendqueue` | 可选 | 渲染队列 `rend_queue`，决定绘制顺序；缺省视为 0 |
| `Renderer::Color` | 可选 | 颜色 `color`，作为着色器中的 `JE_COLOR`；缺省为不透明白色 |

带光源组件（`Light2D::Point` / `Parallel` / `Range`）或 UI 元素组件（`UserInterface::Element`）的实体会被**排除**：光源由 2D 光照管线的光照 pass 处理，UI 元素由 UI 管线处理。

也就是说，实体只要同时具备 `Translation`、`Shape`、`Shaders` 就会被收集并渲染，即使 `Shape` 与 `Shaders` 的内容是空的。两种常见的“兜底”画面：

* **在黑色与品红之间快速往复变化的色块**——着色器列表为空，使用了引擎默认着色器；
* **品红与深灰相间的 2×2 棋盘格**——着色器采样纹理通道 0，但实体没有在通道 0 绑定纹理，采样到了引擎的兜底纹理。

### 着色器、纹理通道与颜色

* `Shaders` 是一个列表：实体的网格会按列表顺序，用每个着色器**各绘制一次**（多遍绘制）。
* 纹理按*纹理通道*（pass id）绑定，通道号对应着色器中声明的纹理槽位——例如内置 `Unlit.shader` 把主纹理 `Main` 声明在通道 0。绘制前引擎先在通道 0 绑定兜底纹理，再绑定实体自己的纹理，实体绑在通道 0 的纹理会覆盖兜底纹理；`Textures2DArray` 在 `Textures2D` 之后绑定，同一通道上后者覆盖前者。
* `Textures2D` 的 `tiling` / `offset` 作为 `JE_UV_TILING` / `JE_UV_OFFSET` 传给着色器；`Color` 转换到线性空间后作为 `JE_COLOR` 传入。内置 `Unlit.shader` 输出 `JE_COLOR × 纹理颜色`，因此 `Color` 可以直接给实体染色。
* 引擎**没有独立的材质概念**：深度测试、深度写入、混合方式、面剔除等状态都由着色器声明。例如内置 `UnlitBlend.shader` 声明 `BLEND (ADD, SRC_ALPHA, ONE_MINUS_SRC_ALPHA)` 并关闭深度写入，用于半透明物体。

⚠️ 注意 `Shaders::set_uniform` 修改的是**着色器资源本身**的 uniform 值，不是实体私有的副本。多个实体引用同一个着色器实例时——把同一个已加载的着色器放进多个实体的列表，或声明了 `SHARED (true)` 的着色器在加载时带上宇宙（Woolang）/ 图形上下文（C++）而按路径复用同一实例——修改对它们全部生效。

### 渲染顺序：Rendqueue

渲染顺序由 `Renderer::Rendqueue` 的 `rend_queue` 决定，分两个层面：

* **同一摄像机内**：待渲染实体按 `rend_queue` 升序排序后依次绘制。排序是稳定的，队列值相同的实体之间保持切片的收集顺序——它取决于实体在内部存储中的排列，**不应依赖**；
* **摄像机之间**：摄像机实体自身的 `Rendqueue` 决定该摄像机画面的绘制先后，同样是升序（见[摄像机](camera.md)）。

引擎**不会**按实体与摄像机的距离排序。半透明物体通常使用关闭深度写入的混合着色器，需要画在不透明物体之后时，应显式给它更大的 `rend_queue`。

### 渲染发生在一帧的什么时候

渲染系统的更新函数是 `GraphicUpdate`，它在系统更新的**最后一个阶段**执行（见[世界·系统更新的八个阶段](world.md#系统更新的八个阶段)），此时本帧的脚本逻辑、变换结算、物理模拟都已完成：颜色、纹理、渲染队列等渲染组件在任何脚本阶段的修改，都会体现在当帧画面中。

⚠️ 注意 变换结算在 UPDATE 之后、LATE_UPDATE 之前：在 LATE_UPDATE 中修改 `LocalPosition` 等局部变换组件，要到下一帧才会结算进 `Translation` 并反映到画面上（见[变换](transform.md)）。

渲染系统在这一阶段只是*录制*绘制命令；命令在[帧](frame.md)的提交任务阶段交给图形线程执行，宇宙会等这一帧的绘制完成后才进入下一帧。

### 自定义绘制

内置管线之外，也可以直接录制绘制命令：从图形宿主申请*渲染分支*，每帧以一个优先级开始新的一帧，再从分支申请指向屏幕或[帧缓冲](framebuffer.md)的*命令缓冲*，在其中绑定着色器、纹理并绘制。编辑器的 Gizmo 就是这样绘制的（使用 1000000 的优先级，保证总在最后绘制）。Woolang 侧对应 `je::graphic` 中的 `graphic_uhost` / `render_branch` / `command_buffer`；C++ 侧可以继承 `jeecs::graphic::BasePipelineInterface` 编写自己的渲染系统。

### API 速览

#### Woolang（`je::graphic` / `je::ecs` / `je::towoo`）

| 接口 | 说明 |
| --- | --- |
| `graphic::shader::load(univ, path)` / `graphic::texture2d::load(univ, path)` | 加载着色器 / 2D 纹理，返回 `option` |
| `graphic::vertex::load(univ, path)` / `graphic::vertex::create(...)` | 加载或程序化创建网格 |
| `entity->set_shaders(shaders)` / `get_shaders()` | 设置 / 读取实体的着色器列表 |
| `entity->bind_texture(pass, tex)` / `get_textures()` | 绑定纹理到通道（`tex` 为 `none` 时解除）/ 读取“通道→纹理”映射 |
| `entity->set_shape(vertex)` / `get_shape()` | 设置网格（`none` 时回退默认四边形）/ 读取网格 |
| `Renderer::Shaders::set_shaders / get_shaders / set_uniform` | towoo 组件实例上的着色器操作 |
| `Renderer::Textures2D::bind_texture / get_texture`、`Renderer::Shape::set_vertex / get_vertex` | towoo 组件实例上的纹理与网格操作 |
| `rend_queue`、`color`、`tiling`、`offset` | `Rendqueue` / `Color` / `Textures2D` 的反射成员，经 `->get` / `->set` 读写 |

⚠️ 注意 实体级接口（`entity->set_shaders` 等）作用于实体上**已生效**的组件：实体没有对应组件时调用被静默忽略，同一帧刚添加、尚未生效的组件也一样。需要在添加组件的当帧设置内容时，应改用 `entity->towoo::add_component:<T>()` 返回的组件实例上的接口（编辑器切换 2D 光照模式时即如此处理）。

在 towoo 系统中修改渲染相关组件：

```woolang
UPDATE!
{
    // 带 Shaders 与 Textures2D 的实体；Color 可有可无（类型后缀 ?）
    system! retint(
        shaders: Renderer::Shaders,
        textures: Renderer::Textures2D,
        tint: Renderer::Color?,
    )
    {
        shaders->set_uniform("u_strength", 0.5);    // 写入该实体的每个着色器
        textures.tiling->set((2., 2.) as vec2);      // UV 平铺两次
        match (tint)
        {
            value(c)? c.color->set((1., 0.5, 0.5, 1.) as color);
            none?;
        }
    }
}
```

#### C++（`jeecs::Renderer`，头文件 `jeecs/components.hpp`；资源封装见 `jeecs/graphic.hpp`）

| 成员 / 方法 | 说明 |
| --- | --- |
| `Shape::vertex` | `optional<resource<graphic::vertex>>`，`reset()` 后回退默认四边形 |
| `Shaders::shaders` / `Shaders::set_uniform(name, val)` | 着色器列表 / 向列表中每个着色器写入 uniform |
| `Textures2D::bind_texture(pass, tex)` / `remove_texture(pass)` / `get_texture(pass)` | 纹理通道的绑定、解除与查询 |
| `Textures2D::tiling` / `offset`、`Rendqueue::rend_queue`、`Color::color` | UV 平铺与偏移、渲染队列、颜色 |
| `graphic::shader::load(context, path)` / `graphic::texture2d::load(context, path)` | 加载资源，返回 `std::optional<basic::resource<...>>`；`context` 可为 `nullptr` |

```cpp
using namespace jeecs;
using namespace jeecs::slice_requirement;

struct BlendSystem : public game_system
{
    basic::resource<graphic::shader> m_blend =
        graphic::shader::load(nullptr, "!/builtin/shader/UnlitBlend.shader").value();

    BlendSystem(game_world w) : game_system(w) {}

    void Update()
    {
        for (auto&& [shaders, textures, rendqueue] : query<view typesof(
            Renderer::Shaders&, Renderer::Textures2D&, Renderer::Rendqueue*)>())
        {
            if (shaders.shaders.empty())
                shaders.shaders.push_back(m_blend);   // 半透明混合着色器
            textures.tiling = math::vec2(2.f, 2.f);
            if (rendqueue != nullptr)
                rendqueue->rend_queue = 100;          // 画在不透明物体之后
        }
    }
};
```

### 相关术语

参见[术语表](README.md)：

* [摄像机](camera.md)——渲染系统为每个摄像机渲染一次全部待渲染实体；
* [帧缓冲](framebuffer.md)——摄像机除屏幕外的另一种渲染目标；
* [着色器](shader.md)、[纹理](texture.md)、[网格](mesh.md)——待渲染实体引用的图形资源；
* [系统](system.md)、[切片](slice.md)——渲染系统的本质与它定位实体的方式；
* [帧](frame.md)、[世界](world.md)——渲染在一帧中的执行时机；
* [UI](ui.md)、[光照](light.md)——另外两条内置管线负责的内容。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。三条内置管线位于 `src/jeecs_core_graphic_system.{hpp,cpp}`（公共基类）、`src/jeecs_core_graphic_unlit_system.cpp`、`src/jeecs_core_graphic_defer_light2d_system.cpp` 与 `src/jeecs_core_graphic_ui_system.cpp`；图形宿主与渲染分支见 `src/jeecs_graphic_host.cpp`，命令缓冲见 `src/jeecs_graphic_cmd.cpp`，图形线程与资源生命周期见 `src/jeecs_graphic_api_basic.cpp`，后端虚表、C API 与 C++ 封装见 `include/jeecs/graphic.hpp`。

### 管线系统的类层次

```
game_system
 └─ graphic::BasePipelineInterface    持有图形宿主（uhost），管理本系统的渲染分支
     └─ GraphicPipelineBase            兜底资源、pipeline 缓存、摄像机矩阵、通用绘制
         ├─ UnlitGraphicPipelineSystem
         ├─ DeferLight2DGraphicPipelineSystem
         └─ UserInterfaceGraphicPipelineSystem
```

三者在 `src/jeecs_core_systems_and_components.cpp` 中以 `Graphic::*` 类型名注册，都只实现 `GraphicUpdate`，因此只在 `m_graphic_update` 阶段工作。`BasePipelineInterface::OnDisable` 释放本系统持有的全部渲染分支，这就是世界停用后其画面消失的原因。

### 一帧的工作流（以 Unlit 为例）

`UnlitGraphicPipelineSystem::GraphicUpdate` 依次执行：

1. `begin_frame()`：读取窗口尺寸，`branch_allocate_begin()` 重置分支分配计数，并由 `update_camera_matrices()` 更新全部摄像机的视图 / 投影矩阵与视锥平面（见[摄像机](camera.md)）；
2. `collect_cameras()`：查询所有带 `Camera::Projection` 的实体，每个摄像机 `allocate_branch(rend_queue)` 取得一个渲染分支（优先级即摄像机的 `rend_queue`），再按 `rend_queue` 稳定排序；
3. 按第一部分的切片收集待渲染实体（`world_renderer_arch`，可选组件为空指针），`sort_by_rend_queue` 稳定排序；
4. `end_frame()`：`branch_allocate_end()` 回收本帧未用到的分支；
5. `DrawFrame()`：逐摄像机先 `resolve_camera_target`（`RendToFramebuffer` 存在但未设置帧缓冲时**本帧跳过该摄像机**），再 `update_default_uniform_buffer` 写入 V / P / VP 与着色器时间，然后 `begin_camera_pass`——申请命令缓冲（视口 = `Viewport` 归一化矩形 × 目标像素尺寸）、有 `Clear` 时清颜色附件 0、**总是**把深度清为 1.0、绑定摄像机的 uniform 缓冲——最后 `draw_world_renderers`。

`draw_world_renderers` 对每个实体：摄像机带 `FrustumCulling` 时先做包围球剔除；计算 `MV = V × M`、`MVP = VP × M`（M 取 `Translation::object2world`）；`bind_entity_textures` 按“通道 0 兜底纹理 → `Textures2D` → `Textures2DArray`”的顺序绑定；`draw_shader_passes` 遍历着色器列表（空列表换成默认着色器列表），为每个着色器绑定 pipeline 与顶点，只为着色器**实际声明**的内置 uniform 赋值（`JE_CHECK_NEED_AND_SET_UNIFORM`），然后 `draw_indexed`。

pipeline 由 `pipeline_for` 按 **(着色器, 拓扑)** 缓存于 `m_pipeline_cache`：固定功能状态取自着色器（`je_graphic_pipeline_state_from_shader`），拓扑取自网格类型（线带 / 三角形 / 三角形带）；系统析构时统一关闭。

### DeferLight2D 与 UI 的差异

* **DeferLight2D**：摄像机查询额外要求 `Translation`，并读取 `Light2D::CameraPostPass`、`Shaders`、`Textures2D` 与 `Color`。带 `CameraPostPass` 的摄像机走延迟路径（合成 pass 直接使用摄像机的 `Shaders`，因此这类摄像机必须同时挂 `Renderer::Shaders`）：尺寸变化时重建 G-buffer（漫反射、自发光、视空间坐标、视空间法线四个颜色附件 + 深度）与按 `light_rend_ratio` 缩放的光照缓冲；每帧先把各光源的阴影画进其 `ShadowBuffer`，再把场景画进 G-buffer、把光照画进光照缓冲，最后用摄像机自身的 `Shaders` 以全屏四边形合成到摄像机的渲染目标。不带 `CameraPostPass` 的摄像机与 Unlit 一样直接绘制。
* **UI**：收集带 `UserInterface::Element`、`WorldLayout`、`Shaders` 且带 `Shape` 或 `Text` 的实体；不做视锥剔除；投影为单位矩阵，视图矩阵把摄像机目标的像素坐标映射到 NDC；逐摄像机更新 `MouseOn` 命中状态；文本按（字形图集页, 着色器）合批写入动态顶点缓冲。

### 录制与提交：图形宿主、渲染分支、命令缓冲

* **图形宿主**（`je_graphic_uhost_t`）：每个宇宙至多一个，在首次经 `je_graphic_uhost_get_or_create_for_universe` 请求时惰性创建（管线系统在 `BasePipelineInterface` 构造时请求）。它启动图形线程（后端的 `interface_startup` 负责创建窗口；未提供配置时为 640×480 窗口、可缩放、垂直同步、MSAA×4），并在宇宙上注册一个提交任务（after call-once job）；宇宙退出时经退出回调释放。
* **渲染分支**（`je_render_branch_t`）：一组命令缓冲 + 一个优先级。`new_frame(priority)` 清空本帧计数并设置优先级；申请命令缓冲时复用池中实例，`reset` 后立即以目标（空指针 = 屏幕）和视口 `begin_pass`。`BRANCH_CHAIN_POOL_SIZE = 1`，即单缓冲——源码注释说明分支内的 uniform 缓冲只有一份，双缓冲没有意义。
* **命令缓冲**（`je_gl_command_buffer_t`）：录制可在任意线程进行，只写 CPU 内存（命令流 + uniform 区），并**持有所有被引用资源的引用**直到 `reset` / `close`；提交只在图形线程进行，按需初始化 / 更新资源后把命令流分发给后端虚表。

```
常规任务（第 2 步）  各管线的 GraphicUpdate：收集、排序、录制命令缓冲
提交任务（第 3 步）  uhost 的提交任务调用 je_graphic_update(JE_GL_WAIT_THIS_FRAME_END)，
                     唤醒图形线程并阻塞到本帧绘制完毕
图形线程             update_frame_ready（后端交换呈现上一帧并准备本帧，可返回 SKIP / STOP）
                     → 帧任务 _frame_rend_impl → update_draw_commit（提交设备，GUI 按约定也在此绘制）
```

`_frame_rend_impl` 在正常帧先把窗口清为 (0,0,0,0)、深度清为 1.0，再把宇宙内**所有**分支（跨世界、跨管线）按优先级升序稳定排序，依次提交各分支的命令缓冲。后端请求跳过本帧（如窗口最小化）时默认跳过全部绘制；`je_graphic_uhost_set_skip_behavior(host, false)` 可改为只跳过以屏幕为目标的命令缓冲。

这一时序与[世界](world.md#宇宙主循环中的一帧)中“呈现必须在下一帧世界更新之前完成”的约束一致。在 jedriver 中，图形线程就是主线程（`game_engine_context::loop()` → `graphic_syncer_host::loop()`），宇宙循环运行在自己的线程上。

⚠️ 注意 同优先级分支的先后不作保证：全局分支表的初始顺序是分配顺序，而同一阶段的多个管线系统并行执行，分配顺序本身就不确定。每条管线都会遍历本世界的**全部**摄像机、各自按 `Clear` 清色，同一摄像机被多条管线使用时，这些分支的优先级相同，彼此先后同样不确定。

### 图形 API 抽象层与后端

渲染系统与命令缓冲都不直接调用具体图形 API，而是经由后端虚表 `je_gl_graphic_api_t`：

| 分组 | 成员 |
| --- | --- |
| 生命周期 | `interface_startup` / `interface_shutdown_before_resource_release` / `interface_shutdown` |
| 帧 | `update_frame_ready` / `update_draw_commit` |
| 资源 | 着色器 / 纹理 / 顶点的 `*_create_blob` / `*_close_blob`；各类资源（含帧缓冲、uniform 缓冲）的 `*_init` / `*_update` / `*_close` |
| 渲染 | `pipeline_*`、`sampler_*`、`begin_pass` / `clear_color` / `clear_depth` / `end_pass`、`bind_*`、`draw` / `draw_indexed`、`commit_begin` / `commit_end` |

资源在首次被使用时由图形线程惰性初始化，句柄的 `m_modified` 置位后在下次使用前更新；引用计数归零后排队，由图形线程在适当时机关闭。图形线程上下文 `je_gl_context_t` 持有配置、虚表指针与后端实现上下文；窗口由 `jeecs_graphic_api_interface.hpp` 中的 `basic_interface` 抽象，桌面平台用 GLFW、Android 用 EGL 实现。

| 后端 | 实现文件（`src/`） | ImGui 后端（`src/`） |
| --- | --- | --- |
| 无图形输出 | `jeecs_graphic_api_impl_none.cpp` | `jeecs_graphic_imgui_backend_none.cpp` |
| OpenGL 3.3 / OpenGLES 3.0 / WebGL 2.0 | `jeecs_graphic_api_impl_opengl3.cpp`（三者共用） | `jeecs_graphic_imgui_backend_opengl3.cpp` |
| DirectX 11 | `jeecs_graphic_api_impl_dx11.cpp` | `jeecs_graphic_imgui_backend_dx11.cpp` |
| Vulkan 1.2 | `jeecs_graphic_api_impl_vk120.cpp` | `jeecs_graphic_imgui_backend_vk120.cpp` |
| Metal | `jeecs_graphic_api_impl_metal.cpp` | `jeecs_graphic_imgui_backend_metal.cpp` |

* **编译期**：CMake 选项 `JE4_ENABLE_{DX11,OPENGL330,OPENGLES300,WEBGL20,VULKAN120,METAL}_GAPI`。OpenGL 3.3、OpenGLES 3.0、WebGL 2.0 三者互斥（ImGui 无法同时链接两套 GL 后端，CMake 配置阶段直接报错）；DX11、Vulkan、Metal 可与它们自由组合。
* **运行期**：命令行参数 `-gapi dx11|gl3|vk120|metal|none` 选择已编译的后端；未指定时按 DX11 → Metal → Vulkan → OpenGL 系 → none 的顺序取第一个已启用者。
* **新增后端** = 一个填写 `je_gl_graphic_api_t` 的 `jeecs_graphic_api_impl_*.cpp` + 一个对应的 ImGui 后端文件。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| Renderer 组件定义 | `include/jeecs/components.hpp`（`Renderer` 命名空间） |
| 管线公共基类（兜底资源、摄像机矩阵、通用绘制） | `src/jeecs_core_graphic_system.{hpp,cpp}` |
| Unlit / DeferLight2D / UI 管线 | `src/jeecs_core_graphic_{unlit,defer_light2d,ui}_system.cpp` |
| 管线系统注册 | `src/jeecs_core_systems_and_components.cpp` |
| 图形宿主、渲染分支 | `src/jeecs_graphic_host.cpp` |
| 命令缓冲录制与提交 | `src/jeecs_graphic_cmd.cpp`（设计说明见 [render_api_v2.md](../capi/graphic/render_api_v2.md)） |
| 图形线程、资源生命周期、后端选择 | `src/jeecs_graphic_api_basic.cpp` |
| 后端虚表、C API、C++ 封装 | `include/jeecs/graphic.hpp`（逐项函数说明见 `doc/capi/graphic/`） |
| Woolang 封装 | `je/graphic.wo`、`je/ecs.wo`（实体级接口）、`je/towoo/towoo.wo`（组件实例接口） |
| 内置着色器与兜底着色器 | `builtin/shader/`、`builtin/shader/internal/builtin_default.shader` |
| 自定义绘制的实践参考 | `builtin/editor/Editor/WorldEditorViewport/GizmoDrawing.wo` |
