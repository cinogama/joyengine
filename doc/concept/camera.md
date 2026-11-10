# 摄像机（Camera）

本文档是[术语表](README.md)中「摄像机」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述摄像机实体的组件构成、投影方式与输出目标；
* **第二部分·底层实现**——面向引擎维护者，描述投影矩阵的结算、视锥剔除的实现与摄像机在渲染流程中的位置。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 摄像机是什么

*摄像机*（Camera）不是引擎中的特殊对象，而是**带有特定摄像机组件的普通实体**：给实体挂上 `Camera::Projection` 与一个投影组件，它就成为摄像机。[渲染系统](render.md)每帧通过切片找到本世界的所有摄像机，**为每个摄像机渲染一次全部待渲染实体**。摄像机没有全局唯一的限制：一个世界可以同时有多个摄像机，各自渲染一遍场景。配合 `Viewport` 可以把多个摄像机的画面放到屏幕的不同区域（分屏）；配合 `RendToFramebuffer` 可以让摄像机渲染进[帧缓冲](framebuffer.md)，供其他地方当作纹理使用。

### 摄像机实体的组件构成

| 组件 | 要求 | 作用 |
| --- | --- | --- |
| `Transform::Translation` | 必须 | 摄像机的世界位置与朝向，决定视图矩阵（见[变换](transform.md)） |
| `Camera::Projection` | 必须 | 裁剪距离 `znear` / `zfar`；并持有本摄像机的矩阵缓存与 uniform 缓冲 |
| `Camera::OrthoProjection` / `Camera::PerspectiveProjection` | 二选一 | 正交或透视投影（见下） |
| `Camera::Viewport` | 可选 | 画面在渲染目标上的归一化区域，缺省为整个目标 |
| `Camera::Clear` | 可选 | 渲染本摄像机前的清屏颜色 |
| `Camera::FrustumCulling` | 可选 | 开启视锥剔除 |
| `Renderer::Rendqueue` | 可选 | 摄像机之间的绘制先后（见[渲染](render.md)） |
| `Camera::RendToFramebuffer` | 可选 | 把渲染目标从屏幕改为指定的[帧缓冲](framebuffer.md) |

参照编辑器创建调试摄像机时的组件组合，一个正交摄像机可以这样创建：

```woolang
import je;
import je::ecs;
import je::typeinfo;
import je::towoo::components;

let w = je::world::create(current_universe);
do w->add_system(je::typeinfo::load("Graphic::UnlitGraphicPipelineSystem")->unwrap);  // 渲染系统
do w->add_entity([
    Transform::Translation::type::typeinfo, Transform::LocalToWorld::type::typeinfo,
    Transform::LocalPosition::type::typeinfo, Transform::LocalRotation::type::typeinfo,
    Camera::Projection::type::typeinfo, Camera::OrthoProjection::type::typeinfo,
    Camera::Clear::type::typeinfo,
]);
w->set_enable(true);
```

⚠️ 注意 投影组件应**恰好挂一个**。一个都没有时，摄像机仍会被收集、仍会按 `Clear` 清屏，但它的矩阵永远不会被计算；两个都挂时，渲染与编辑器（射线拾取、世界网格、调试摄像机控制）统一按**正交**处理。

### 两种投影

| 组件 | 参数 | 行为 |
| --- | --- | --- |
| `Camera::OrthoProjection` | `scale`，默认 1 | 正交投影，物体不随远近缩放。可视高度为 `10 / scale` 个世界单位，可视宽度 = 可视高度 × 画面宽高比；`scale` 越大视野越小。适合 2D 游戏 |
| `Camera::PerspectiveProjection` | `angle`，默认 75（度） | 透视投影，`angle` 为**垂直**视场角。适合 3D 场景 |

两种投影共用 `Projection` 的 `znear`（默认 0.3）与 `zfar`（默认 1000）。宽高比取自摄像机实际占用的像素区域（`Viewport` × 渲染目标尺寸），因此窗口缩放、换用帧缓冲或修改 `Viewport` 时，**垂直可视范围不变，水平方向随宽高比伸缩**。正交与透视可以通过增删投影组件在运行时切换（编辑器的 2D/3D 模式即如此），组件增删下一帧生效。

### Viewport：画面占用的区域

`Camera::Viewport` 的 `viewport` 是 `(x, y, width, height)`，**全部为归一化值**（0~1），**原点在左下角**、y 轴向上，缺省为 `(0, 0, 1, 1)`。它被换算为渲染目标上的像素矩形，世界画面完整映射进这个矩形，宽高比也按它计算，画面不会被拉伸。例如两个摄像机分别使用 `(0, 0, 0.5, 1)` 与 `(0.5, 0, 0.5, 1)`，就是左右分屏。

### Clear：清屏

挂 `Camera::Clear` 的摄像机在渲染前，把渲染目标的颜色附件 0 清为 `color`（RGBA，各分量 0~1）；深度缓冲则无论是否挂 `Clear`，每个摄像机渲染前都会被清为 1.0。不挂 `Clear` 时：渲染到**屏幕**不会残留上一帧——引擎每帧提交任何绘制之前已把整个屏幕清为黑色（RGBA 全 0）；渲染到**帧缓冲**则不会被自动清除，上一帧的内容会保留。

⚠️ 注意 清除的是**整个**颜色附件，**不受 `Viewport` 限制**。分屏时如果两个摄像机都挂 `Clear`，后绘制的摄像机会把先绘制的画面一并清掉——只给最先绘制（`rend_queue` 最小）的摄像机挂 `Clear`。

### FrustumCulling：视锥剔除

挂上 `Camera::FrustumCulling` 后，渲染系统每帧从该摄像机的视图投影矩阵解算出 6 个视锥平面，绘制每个待渲染实体前先做包围球测试（球心为实体世界位置，半径按缩放后的网格包围盒估算），完全落在视锥外的实体直接跳过、不产生绘制调用。不挂此组件则不做剔除。

### 渲染目标：屏幕或帧缓冲

不挂 `RendToFramebuffer` 时画面输出到屏幕；挂上并设置帧缓冲后，画面写入该[帧缓冲](framebuffer.md)，可以作为纹理继续使用——编辑器的场景视口就是调试摄像机渲染进帧缓冲、再把颜色附件画进视口窗口。挂了组件但**尚未设置**帧缓冲的摄像机会被跳过，因此可以先挂组件，等帧缓冲创建好再设置。

### 摄像机与 UI

[UI](ui.md) 元素没有世界坐标，其矩形按**摄像机渲染目标的像素尺寸**解析，同一个 UI 在不同摄像机下按各自的目标尺寸布局。鼠标命中状态 `UserInterface::MouseOn` 也逐摄像机计算：只有**渲染到屏幕**的摄像机参与（帧缓冲是离屏目标，窗口鼠标不在其空间中），鼠标须落在该摄像机的 `Viewport` 内；命中状态每帧先清零再累计，任一摄像机命中即为 true。

### API 速览

#### Woolang（`je::towoo`，组件实例上的接口）

| 接口 | 说明 |
| --- | --- |
| `entity->towoo::get_component:<Camera::Projection>()` | 获取组件实例（`option`） |
| `Projection::get_view_mat` / `get_proj_mat` / `get_view_proj_mat` / `get_inv_proj_mat` | 读取视图 / 投影 / 视图投影 / 逆投影矩阵（`mat4`，只读） |
| `Projection::get_default_uniform_buffer` | 本摄像机的 uniform 缓冲；自定义绘制时绑定它，着色器即可读到摄像机矩阵 |
| `RendToFramebuffer::get` / `set` | 读取 / 设置渲染目标帧缓冲（`option`，见[帧缓冲](framebuffer.md)） |
| `towoo::ray::from(trans, proj, ndc, ortho)` | 由摄像机与 NDC 坐标构造拾取[射线](ray.md) |
| `znear`、`zfar`、`scale`、`angle`、`viewport`、`color` | 各组件的反射成员，经 `->get` / `->set` 读写 |

在 towoo 系统中操作摄像机：

```woolang
UPDATE!
{
    // 缩小正交摄像机的视野；带 Viewport 时把画面放到屏幕左半边
    system! zoom(ortho: Camera::OrthoProjection, viewport: Camera::Viewport?)
    {
        ortho.scale->set(ortho.scale->get * 0.9);
        match (viewport)
        {
            value(vp)? vp.viewport->set((0., 0., 0.5, 1.) as vec4);
            none?;
        }
    }
}
```

#### C++（`jeecs::Camera`，头文件 `jeecs/components.hpp`）

| 成员 | 说明 |
| --- | --- |
| `Projection::znear` / `zfar`、`Projection::default_uniform_buffer` | 裁剪距离、本摄像机的 uniform 缓冲 |
| `Projection::view` / `projection` / `view_projection` / `inv_projection` | 矩阵缓存（`float[4][4]`），由渲染系统每帧覆写，只读 |
| `OrthoProjection::scale`、`PerspectiveProjection::angle`、`Viewport::viewport`、`Clear::color` | 投影参数、视口、清屏颜色 |
| `FrustumCulling` | 6 个视锥平面，由渲染系统每帧写入；`test_circle(center, r)` 做包围球测试 |
| `RendToFramebuffer::framebuffer` | `basic::optional<basic::resource<graphic::framebuffer>>`，渲染目标 |

```cpp
using namespace jeecs;
using namespace jeecs::slice_requirement;

struct SplitScreenSystem : public game_system
{
    SplitScreenSystem(game_world w) : game_system(w) {}

    void Update()   // 带 Viewport 的透视摄像机：左半屏，垂直视场角 60 度
    {
        for (auto&& [persp, viewport] : query<view typesof(
            Camera::PerspectiveProjection&, Camera::Viewport&)>())
        {
            persp.angle = 60.f;
            viewport.viewport = math::vec4(0.f, 0.f, 0.5f, 1.f);
        }
    }
};
```

### 相关术语

参见[术语表](README.md)：

* [渲染](render.md)——为每个摄像机渲染一次全部待渲染实体的过程；
* [帧缓冲](framebuffer.md)——摄像机除屏幕外的渲染目标；
* [射线](ray.md)——由摄像机的投影参数与屏幕坐标构造；
* [变换](transform.md)——摄像机实体的位置与朝向；
* [UI](ui.md)——按摄像机目标尺寸布局的界面元素。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。摄像机组件定义于 `include/jeecs/components.hpp`（`Camera` 命名空间）；矩阵结算、视锥平面与摄像机 pass 位于 `src/jeecs_core_graphic_system.cpp`（`GraphicPipelineBase`）；投影矩阵公式见 `include/jeecs/graphic.hpp`；射线构造见 `include/jeecs/ray.hpp`。

### Projection 的结构与拷贝语义

`Projection` 除 `znear` / `zfar`（反射注册的仅此二者）外，还持有矩阵缓存 `view` / `projection` / `view_projection` / `inv_projection`（每帧由渲染系统整体覆写，是输出而非输入），以及构造时创建、绑定位置 0 的 `default_uniform_buffer`——布局为 `BasePipelineInterface::default_uniform_buffer_data_t`（V、P、VP 与时间向量），着色器中对应 `JE_V` / `JE_P` / `JE_VP` / `JE_TIME`。它的拷贝构造拷贝 `znear` / `zfar`，矩阵缓存按默认初始化（每帧重算，无需拷贝）；uniform 缓冲则各自新建而不是与源摄像机共享——`basic::resource` 是共享指针，若逐成员拷贝，两个摄像机将共用同一份缓冲并互相覆盖矩阵。

### 矩阵结算

每条管线在 `begin_frame()` 中调用 `update_camera_matrices()`，遍历本世界中带 `Translation`、`Projection` 且带两种投影组件之一（`anyof`）的摄像机：

1. `view = 逆旋转 × 平移(-world_position)`，取自 `Translation` 的世界位置与朝向（变换结算阶段早于图形阶段，用的是本帧结果）；
2. 以 `Viewport` 宽高 × 目标尺寸（帧缓冲已设置时取帧缓冲，否则取窗口）为画面像素尺寸，`OrthoProjection` 存在则调用 `ortho_projection` / `ortho_inv_projection`，否则调用透视版本——这就是“两个投影组件都挂时按正交处理”的来源；
3. 把 view / projection 写入 `default_uniform_buffer`，并计算 `view_projection = projection × view`；
4. 带 `FrustumCulling` 时，由 `view_projection` 的元素组合出左、右、上、下、近、远 6 个平面，经 `set_plane_normal_and_distance` 归一化后写入组件。

正交投影的半高为 `ORTHO_PROJECTION_RATIO / scale`（常量 5.0），半宽再乘宽高比；透视投影以 `angle` 为垂直视场角。三条管线的 `begin_frame` 都执行这一步，同一世界装有多条管线时，摄像机矩阵会被重复计算（结果相同）。

### 视锥剔除的实现与开销

剔除发生在 `draw_world_renderers` 中：`get_entity_size` 取“`local_scale` × 网格局部包围盒（按各轴绝对值最大者对称展开）”的尺寸，以其对角线的一半为半径；`FrustumCulling::test_circle` 依次计算球心到 6 个平面的有符号距离，任一小于 `-r` 即判定在视锥外。这是**逐实体、逐摄像机的 CPU 测试**，每帧每个摄像机各做一遍；包围球对细长或偏心的网格偏大，结果偏保守。UI 管线不做剔除；DeferLight2D 还用它剔除非平行光源。

### 摄像机在渲染流程中的位置

`collect_cameras`（Unlit / UI）收集所有带 `Projection` 的实体（`Rendqueue`、`Viewport`、`RendToFramebuffer`、`FrustumCulling`、`Clear` 均可选），每个摄像机以 `rend_queue`（缺省 0）为优先级分配一个渲染分支，再按 `rend_queue` 稳定排序；DeferLight2D 的摄像机切片另外**要求** `Translation`，并读取 `CameraPostPass` 等组件。

绘制时，`resolve_camera_target` 解析目标：`RendToFramebuffer` 存在但未设置帧缓冲时返回 `nullopt`，本帧跳过该摄像机；否则取帧缓冲或窗口的尺寸。`begin_camera_pass` 随后申请命令缓冲（视口 = `Viewport` × 目标像素尺寸），有 `Clear` 时清颜色附件 0（颜色先转为线性空间），**无条件**清深度为 1.0，并绑定 `default_uniform_buffer`——编辑器自定义绘制 Gizmo 前同样先绑定它。清除命令由后端实现为整附件清除（OpenGL 的 `glClearBufferfv` 且 pass 开始时关闭了 scissor、DX11 的 `ClearRenderTargetView`、Vulkan 以整个帧缓冲为清除矩形），这正是 `Clear` 不受 `Viewport` 限制的原因。屏幕的每帧清除则由图形宿主在提交全部分支之前统一完成。

多摄像机的绘制先后由各自渲染分支的优先级决定：图形宿主把宇宙内的全部分支按优先级稳定排序后依次提交。⚠️ 注意 每条管线都会为本世界的**每个**摄像机分配分支并各自执行 `Clear`；同一摄像机被多条管线（例如 Unlit 与 UI）使用时，这些分支优先级相同，提交先后不作保证，挂 `Clear` 的摄像机可能让后提交的管线清掉先提交的内容。

### 由摄像机构造射线

`math::ray(camera_trans, camera_proj, screen_pos, ortho)` 把 `(x, y, 1, 1)` 乘以 `inv_projection`（w 非零时做透视除法）。**透视**时原点为摄像机世界位置，方向为观察空间方向经摄像机世界旋转后归一化；**正交**时原点为摄像机世界位置加上经旋转的观察空间偏移（逆投影得到的 x、y，z 取 0），方向固定为世界空间 `(0, 0, 1)`，**不随摄像机旋转**。Woolang 侧 `towoo::ray::from` 是它的直接包装；编辑器的实体拾取与各类 Gizmo 都经由它重建视线，世界网格着色器则直接使用逆投影矩阵。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| Camera 组件定义 | `include/jeecs/components.hpp`（`Camera` 命名空间） |
| 矩阵结算、摄像机收集与 pass、视锥剔除 | `src/jeecs_core_graphic_system.cpp`（`update_camera_matrices` / `collect_cameras` / `begin_camera_pass` / `draw_world_renderers`）；投影公式见 `include/jeecs/graphic.hpp`（`ortho_projection` / `perspective_projection` 等） |
| DeferLight2D 摄像机扩展、UI 逐摄像机命中测试 | `src/jeecs_core_graphic_defer_light2d_system.cpp`（`l2dcamera_arch`）、`src/jeecs_core_graphic_ui_system.cpp`（`UpdateMouseOn`） |
| 屏幕每帧清除、分支排序提交 | `src/jeecs_graphic_host.cpp`（`_frame_rend_impl`） |
| 射线构造、towoo 包装与组件声明 | `include/jeecs/ray.hpp`、`je/towoo/towoo.wo`、`je/towoo/components.wo` |
| 编辑器调试摄像机（帧缓冲重建、2D/3D 切换）实践参考 | `builtin/editor/Editor/WorldEditorViewport/`（`WorldOpen.wo`、`CameraControl.wo`） |
