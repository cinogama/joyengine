# UI（UserInterface）

本文档是[术语表](README.md)中「UI」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述 UI 组件的行为约定与用法；
* **第二部分·底层实现**——面向引擎维护者，描述上述行为在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### UI 是什么

UI（UserInterface）是挂在实体上、用于在屏幕上绘制界面元素（按钮底板、图标、文字等）的一族组件。UI 元素**不使用世界坐标**：它的位置与尺寸相对于摄像机的渲染目标（窗口或帧缓冲）来描述——“贴在屏幕右上角、边长为屏幕高度的 20%”这类*屏幕空间*定位，就是术语表所说的“独特的定位方式”。

因此 UI 也走独立的渲染流程：普通渲染管线会**排除**带 `UserInterface::Element` 的实体，UI 由专门的 UI 渲染管线系统绘制。

注意与 [GUI](gui.md) 区分：GUI 是引擎对接的 ImGui 即时模式界面，主要服务于编辑器与调试面板，它不是组件，也不属于任何世界。

### 组件族

| 组件 | 由谁写入 | 职责 |
| --- | --- | --- |
| `UserInterface::Element` | 用户 | 布局输入：锚点、枢轴、偏移与尺寸（绝对 + 相对两个通道）、相对量的单位基准 |
| `UserInterface::WorldLayout` | 系统（用户只读） | 布局解析结果，每帧由布局阶段从 Element 重建；**未挂此组件的元素不参与布局与渲染** |
| `UserInterface::Rotation` | 用户 | 旋转角（度，绕枢轴），会带动整个 UI 子树 |
| `UserInterface::Text` | 用户 | 文本内容、字体、对齐与换行策略 |
| `UserInterface::MouseOn` | 系统写入，用户只读 | 本帧鼠标是否位于该元素上 |

需要画底板或图标的元素还要带渲染组件 `Renderer::Shape` / `Renderer::Shaders` / `Renderer::Textures2D`，可选 `Renderer::Color`（色调）与 `Renderer::Rendqueue`（同一摄像机内按升序依次绘制）。

### 定位方式：锚点、枢轴与两个通道

`Element` 的布局成员：

* `anchor`——参照锚点：偏移的起点位于*参照矩形*的哪个方位（`center`，或 `left`/`right`/`top`/`bottom` 的位组合）；
* `pivot`——枢轴：元素自身盒子上的哪个点对齐到该起点；
* `offset` / `size`——绝对通道，单位像素；
* `offset_ratio` / `size_ratio`——相对通道，按“单位一”折算；
* `unit_kind`——相对通道的单位一，三选一：
  * `height_unit`（默认）——两轴都以参照矩形的**高**为单位一，比例不随宽高比拉伸；
  * `width_unit`——两轴都以参照矩形的**宽**为单位一；
  * `per_axis`——x 轴以宽、y 轴以高为单位一。

最终偏移与尺寸 = 绝对通道 + 相对通道。*参照矩形*：根元素是整个显示区（摄像机的渲染目标），子元素是父 UI 元素的矩形。

例：`anchor` 与 `pivot` 都取 `right | top`，`size_ratio = (0.2, 0.2)`，`offset = (-16, -16)`——得到一个贴在右上角、边长为显示区高度 20%、向内缩进 16 像素的正方形；窗口尺寸变化时它会自动跟随。

### 层级与旋转

UI 元素复用[变换](transform.md)的父子关系（`Transform::Anchor` + `Transform::LocalToParent`）：子元素的锚点相对父元素矩形，父元素的偏移被子元素按原像素量继承。`Rotation::angle` 带动整个子树——子元素绕父枢轴一同旋转，有效角为全部祖先角与自身角之和。父链断开的元素按根元素处理。

### 文本

`Text` 由 UI 管线逐字直接绘制（字形图集 + 批量四边形），不需要 Shape 与 Textures2D：

* `content`——UTF-8 文本，支持内联标记 `{scale:f}`、`{color:RRGGBBAA}`、`{offset:(x,y)}`（均为绝对设置），`\` 转义下一个字符；
* `font`——字体文件，未指定时不渲染；
* `alignment`——文本块在元素矩形内的九宫格对齐（复用 `Element::alignment` 位掩码）；
* `wrap`——`word`（默认，按元素宽度折行，CJK 字符间可断行）或 `none`（不折行，溢出可见）。

**基准字号取自元素尺寸**：默认取元素解析后的高度（`width_unit` 时取宽度），缩放元素即缩放文字。颜色、缩放、偏移标记是逐字的实时属性，修改它们不会触发重新光栅化。`Renderer::Color` 作为整段色调；`Renderer::Shaders` 的第一个着色器作为文本着色器（通常是 `UIText.shader`，也可换成自定义文本特效）。

⚠️ 注意 UI 渲染查询把 `Renderer::Shaders` 列为**必需**组件。组件注释与编辑器“UI文本”组合预设都写着文本“无需任何 Renderer 组件”，但缺少 Shaders 的元素实际不会被绘制；编辑器“创建 → 基本UI文本”预设会挂好 Shaders 并指定 `UIText.shader`，照此配置即可。

### 鼠标交互

给需要交互的元素挂上 `MouseOn`，读取 `is_mouse_on` 即可得知鼠标是否位于元素上（命中测试考虑旋转）：

* 只对渲染到**窗口**的摄像机做测试，渲染到帧缓冲的摄像机是离屏目标，不参与；
* 鼠标须落在摄像机的视口（`Camera::Viewport`）之内；
* 不参与渲染的元素（无 Shape/Text/Shaders）同样参与测试，可当作不可见的按钮热区；
* 被多个摄像机命中时保持为 true。

`MouseOn` 在每帧最后的图形阶段写入，因此脚本在 `UPDATE!` 中读到的是**上一帧**的命中结果。

### 让 UI 显示出来

1. 世界注册 UI 渲染管线系统 `Graphic::UserInterfaceGraphicPipelineSystem` 与变换系统 `Translation::TranslationUpdatingSystem`（布局阶段在后者中执行）；
2. 至少一个摄像机。UI 只需 `Camera::Projection`（编辑器组合预设“仅UI投影”），摄像机的位置、朝向与投影方式都不影响 UI；
3. 创建 UI 实体，编辑器“创建 → 基本UI / 基本UI文本”即为常用组合。

UI 管线会为**所在世界的每一个摄像机**绘制全部 UI 元素，因此建议把 UI 放进单独的世界（见[世界](world.md)）。摄像机的 `Rendqueue` 决定画面的先后：值越大越晚绘制，UI 摄像机取较大的值即可叠在游戏画面之上。

### 脚本用法

#### Woolang（towoo）

```woolang
import je::input;   // 其余导入同 towoo 系统模板

UPDATE!
{
    // 鼠标悬停时把按钮文字染成金色
    system! button_hover(mouse: UserInterface::MouseOn, text: UserInterface::Text)
    {
        text.content->set(
            mouse.is_mouse_on->get
                ? "{color:FFD700FF}开始游戏"
                | "开始游戏");
    }

    // 按窗口尺寸解析元素矩形：(中心, 尺寸, 枢轴偏移)，像素，显示区左下角为原点
    system! log_rect(elem: UserInterface::Element, layout: UserInterface::WorldLayout?)
    {
        let (w, h) = je::input::windowsize();
        let (center, size, _) = elem->get_layout(layout, (w as real, h as real) as vec2);
        je::log(F"center = {center}, size = {size}");
    }
}
```

`Element` 还提供 `mouse_on`（按给定显示区尺寸与鼠标视口坐标做命中测试）与 `get_display_layout`（含祖先旋转链的显示矩形，编辑器的 UI gizmo 即基于它实现）。

#### C++（`jeecs::UserInterface`，头文件 `jeecs/components.hpp`）

```cpp
using namespace jeecs;
using namespace jeecs::slice_requirement;

void MenuSystem::Update()
{
    for (auto&& [mouse, color] : query<view typesof(
        UserInterface::MouseOn&, Renderer::Color&)>())
    {
        color.color = mouse.is_mouse_on
            ? math::color(1.f, 0.85f, 0.2f, 1.f)
            : math::color(1.f, 1.f, 1.f, 1.f);
    }
}
```

需要自行计算矩形时，`Element::resolve_display_rect(layout, w, h)` 把布局结果按给定目标尺寸像素化为 `resolved_rect`（中心/尺寸/枢轴偏移），再用 `resolved_rect::hit_test` 做含旋转的命中测试。

### 相关术语

参见[术语表](README.md)：

* [GUI](gui.md)——编辑器使用的即时模式界面，与 UI 组件是两个概念；
* [变换](transform.md)——UI 复用其父子关系（Anchor + LocalToParent）；
* [渲染](render.md)、[摄像机](camera.md)——UI 是一条独立的渲染管线，按摄像机逐个绘制；
* [帧缓冲](framebuffer.md)——渲染到帧缓冲的摄像机同样绘制 UI，但不做鼠标命中；
* [输入](input.md)——鼠标位置与窗口尺寸来自输入子系统。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。布局阶段位于 `src/jeecs_core_translation_system.cpp`（`UserInterfaceStageUpdate`），绘制与鼠标命中位于 `src/jeecs_core_graphic_ui_system.cpp`，文本逐字渲染器位于 `src/jeecs_graphic_ui_text_renderer.hpp/.cpp`，组件与布局数学见 `include/jeecs/components.hpp` 的 `UserInterface` 命名空间。

### 两段式流程

| 阶段 | 执行者 | 更新槽位 | 工作 |
| --- | --- | --- | --- |
| 布局 | `TranslationUpdatingSystem::UserInterfaceStageUpdate` | `m_transform_update`（紧随普通变换结算） | 把 Element 解析进 WorldLayout |
| 绘制 | `UserInterfaceGraphicPipelineSystem::GraphicUpdate` | `m_graphic_update` | 清零 MouseOn、文本排版、逐摄像机命中测试与绘制 |

布局结果与目标分辨率无关：WorldLayout 存的是系数，绘制端对每个摄像机代入目标宽高即可。

### 布局阶段：分桶与仿射系数

布局阶段**不知道显示区尺寸**，所以把一切量表达为显示区宽高 (w, h) 的函数：

* `ratio_space`（单位一空间）有四个分桶：`absolute`（像素）、`per_axis`（×(w, h)）、`height`（×(h, h)）、`width`（×(w, w)）。“以高为单位一”的横向量无法写成逐轴分数（需要宽高比），必须独立分桶；
* 参考系三通道：`base`（锚定基准点 = 父链基点 + 父枢轴修正 + 自身 anchor）、`offset`（全部祖先自身偏移之和，按**祖先自己的**单位一折算，因此父的 `offset_ratio` 位移被子元素按原像素量继承）、`unit`（参照矩形经 `Element::fold_unit` 按本元素 `unit_kind` 折算）；
* 已复合显示矩形 `center` / `size` / `pivot_offset` / `anchor` 用 `affine2`（k0 + kw·w + kh·h）表达。定点旋转是线性映射，祖先旋转链因此能直接在系数空间复合（`affine2::rotated`）；分桶空间对旋转不封闭，由 `affine2::from_ratio` 的有损并桶完成转换；
* `rotation` = 祖先与自身 `Rotation::angle` 之和，沿父链递推缓存。

求解顺序与普通变换结算相同：先解所有根元素（无 `LocalToParent`）并按 `Anchor::uid` 登记，再反复扫描待定列表，父已解的子元素出列并登记自身，一轮无进展即停止；剩余的孤儿按根语义合成显示矩形且不登记，其子孙同样回退为根。WorldLayout 是唯一的派生状态，输入组件不会被任何系统回写。

### 绘制阶段

`GraphicUpdate` 的顺序：

1. 清零全部 `MouseOn`（逐帧派生态，后续只置位不清零）；
2. 收集摄像机（所有带 `Camera::Projection` 的实体），每个摄像机按其 Rendqueue 分配一个渲染分支；
3. `PrepareTextLayouts`：按**窗口**分辨率重建全部文本的逐字布局（脏检查命中则沿用）；
4. 收集渲染体：`Element&` + `WorldLayout&` + `Shaders&`，并要求 `anyof(Shape, Text)`，按 Rendqueue 稳定排序；
5. `DrawFrame`：逐摄像机解析渲染目标 → `UpdateMouseOn` → 写入 UI 视图矩阵（像素 → NDC，即 2/W、2/H 缩放，投影为单位阵）→ 逐渲染体绘制。

每个元素的模型矩阵为 T(偏移)·R(总角，绕枢轴)·S(尺寸)，把 ±0.5 的单位矩形映射到相机像素空间；偏移项保证枢轴落在其经祖先旋转链后的显示位置。Shape 路径把“UI 视图矩阵 × 模型矩阵”同时作为 MV 与 MVP 交给着色器；同时挂 Shape 与 Text 时先画底板、再画文字。

Unlit 与 DeferLight2D 管线的世界渲染体查询都 `except` 了 `Element`——这就是 UI 与普通实体渲染流程的分叉点。

### 文本逐字渲染

`ui::TextRenderer` 的核心约定：

* 字形按（字体, 量化字号, 码点）光栅化一次，装箱进按字体分页的 1024×1024 RGBA 图集；页内只追加、不重排，已装入字形的 UV 不会变化；全局最多 32 页，超限时淘汰最久未用的页，闲置 600 帧的字体条目整组释放；
* 布局缓存存于 `Text::runtime_layout`（不反射、不序列化），脏检查键为（内容, 字体, 量化字号, 折行宽度, 对齐）；
* 行网格：行高只取决于字体与基准字号，字形按基线放置，垂直对齐锚定字体度量行盒而非墨水包围盒；
* 布局按窗口分辨率量化字号，摄像机目标分辨率不同时由发射端按基准轴比例统一缩放；
* 发射时字形四边形经与 Shape 路径相同的元素矩阵在 CPU 侧烘焙为相机像素空间顶点（位置/UV/颜色 = 逐字颜色 × 节点色调），按（图集页, 着色器）合并为连续运行段，每段一次绘制调用。顶点缓冲池每块 8192 个四边形、最多 32 块，跨帧复用、同帧内每段独占一块；耗尽时丢弃溢出文本并告警一次。

### 鼠标命中

`UpdateMouseOn` 在每个摄像机绘制前执行：非窗口目标直接返回；窗口鼠标坐标（`input::mousepos(0)`）换算为 NDC（y 向上），检查是否落在视口内；再对所有 `Element + WorldLayout + MouseOn` 实体按该摄像机目标尺寸 `resolve_display_rect`，以 `WorldLayout::rotation` 调用 `hit_test`（鼠标相对中心的差值反向旋转后与半尺寸比较），命中只置位。

towoo 的 `get_layout` / `mouse_on` / `get_display_layout` 由 `src/jeecs_woolang_towoo_script_basic.cpp` 实现，未传 WorldLayout 时按根元素回退（anchor 相对显示区解析）。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 组件定义与布局数学（`ratio_space` / `affine2` / `resolve_*` / `hit_test`） | `include/jeecs/components.hpp`（`UserInterface`） |
| 布局阶段 | `src/jeecs_core_translation_system.cpp`（`UserInterfaceStageUpdate`） |
| UI 渲染管线、鼠标命中 | `src/jeecs_core_graphic_ui_system.cpp`、`src/jeecs_core_graphic_system.hpp` |
| 文本逐字渲染器 | `src/jeecs_graphic_ui_text_renderer.hpp/.cpp` |
| towoo 声明与脚本侧解析 | `je/towoo/components.wo`、`je/towoo/towoo.wo`、`src/jeecs_woolang_towoo_script_basic.cpp` |
| 编辑器预设与 UI gizmo | `builtin/editor/Editor/ComponentPresets.wo`、`EntityClipboard.wo`、`WorldEditorViewport/UIGizmo.wo` |
