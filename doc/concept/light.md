# 光照（Light2D）

本文档是[术语表](README.md)中「光照」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述 2D 光照组件的行为约定与用法；
* **第二部分·底层实现**——面向引擎维护者，描述上述行为在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 光照是什么

JoyEngine 内置的光照是一套 *2D 延迟光照*（Light2D）：场景物体先把颜色、自发光、视空间位置与法线写入摄像机的 *G-buffer*，再由每个光源在光照缓冲上叠加贡献，最后由摄像机的合成 pass 把两者合成为最终画面。整个流程由渲染管线系统 `Graphic::DeferLight2DGraphicPipelineSystem` 执行（见[渲染](render.md)）。

光源本身也是实体：它的 `Renderer::Shape` 决定光照覆盖的区域，`Renderer::Shaders` 决定光照的着色方式，光照组件决定光源类型与参数。

### 组件族

| 分组 | 组件 | 挂在 | 主要成员与职责 |
| --- | --- | --- | --- |
| 光源类型（至少一个） | `Light2D::Point` | 光源 | `decay`：从中心到边缘的衰减指数 |
| | `Light2D::Parallel` | 光源 | 平行光：覆盖全屏，方向为实体局部 -Y |
| | `Light2D::Range` | 光源 | `decay` + `shape`：自定义多边形分层光照形状 |
| 光源修饰 | `Light2D::Gain` | 光源 | `gain`：强度增益 |
| | `Light2D::TopDown` | 光源 | 标记为俯视角光源，改变阴影的高度判定与前后排序 |
| | `Light2D::ShadowOffset` | 光源 | `offset`：阴影计算点偏移（光照本身仍从原位置渲染） |
| | `Light2D::ShadowBuffer` | 光源 | **挂上才投射阴影**；`resolution_ratio`、`block_shadow_length` |
| 阴影 | `Light2D::BlockShadow` | 遮挡物 | 多边形轮廓沿背光方向挤出的硬阴影 |
| | `Light2D::ShapeShadow` | 遮挡物 | 以自身贴图的不透明轮廓投下的影子 |
| | `Light2D::SpriteShadow` | 遮挡物 | 脚下椭圆 + 背光尾迹的角色接触阴影 |
| | `Light2D::WallShadow` | 遮挡物 | 把精灵背光边沿挤出方向抹成条带，适合墙体、箱柜 |
| | `Light2D::SelfShadow` | 遮挡物 | 把实体自身覆盖为阴影色（背光时变暗） |
| | `Light2D::PerBlockSelfShadow` | 遮挡物 | 与 SelfShadow 同挂，供瓦片地图等分块大网格逐块判定 |
| 输出 | `Light2D::CameraPostPass` | 摄像机 | 开启该摄像机的延迟光照；`light_rend_ratio`：光照缓冲分辨率比例 |

### 光源

* **点光（Point）**：光照区域就是光源的 Shape（默认单位四边形，随缩放变大），强度从中心向边缘按 `pow(1 - 距离比例, decay)` 衰减，`decay` 越大衰减越快；
* **平行光（Parallel）**：着色器把（默认的单位）四边形铺满屏幕，方向取实体局部 -Y（默认朝屏幕下方），旋转实体即可改变方向；
* **范围光（Range）**：`shape` 由若干“层”组成，每层有相同数量（`point_count`）的轮廓点与一个强度（`strength`），系统在相邻层之间连成网格，强度在层间插值后再按 `decay` 取幂——用来做任意形状的柔和光区。

光的颜色来自光源的 `Renderer::Color`，最终强度 = Color 的 RGB × Color 的 A × `Gain::gain`。点光与范围光的贡献还要除以 (d + 1)（d 为同一像素上表面与光源网格的视空间距离）；三种光源都按表面法线与光线方向的夹角调制。

### 阴影

光源挂了 `ShadowBuffer` 才会投射阴影；`resolution_ratio`（默认 0.5）是阴影缓冲相对窗口尺寸的比例，`block_shadow_length`（默认 2000 世界单位）是 BlockShadow 的挤出长度。

各阴影组件都有 `factor`（强度）：BlockShadow、ShapeShadow、SpriteShadow、WallShadow 在 `factor` ≤ 0 时不绘制；SelfShadow 则总会作为“接收面”参与绘制（受光时写 0，借此清除位于其后方的阴影）。其它要点：

* **BlockShadow**：`shape` 是闭合轮廓点列（默认单位正方形）；`reverse` 切换为剔除正面的着色器变体，用于绕序相反的轮廓；
* **ShapeShadow**：`distance` 控制影子被推离的程度，`tiling_scale` 调整采样贴图的平铺；
* **SpriteShadow**：`ellipse_major` / `ellipse_minor` 为椭圆轴长，`distance` 为尾迹长度（≤0 只画椭圆），`fade_distance` 为尾迹消隐距离（≤0 取 `distance`）；
* **WallShadow**：`distance` 为条带延伸长度。

**高度判定**：BlockShadow、ShapeShadow、SelfShadow 的 `auto_disable`（默认开启）会按“实体与光源的高度关系”自动开关——普通光源比较两者的世界 Z，TopDown 光源比较实体包围盒底边 Y 与光源 Y。BlockShadow 与 SelfShadow 在实体低于光源时生效，ShapeShadow 则在实体不低于光源时生效。关闭 `auto_disable` 即无条件生效。

### 最小配置

1. 世界注册 `Translation::TranslationUpdatingSystem` 与 `Graphic::DeferLight2DGraphicPipelineSystem`（替代 Unlit 管线）；
2. 摄像机挂 `Light2D::CameraPostPass` 与 `Renderer::Shaders`，着色器选合成着色器 `CameraDefer2DPass.shader`（另有 Bloom / Fog 变体）；
3. 场景物体使用 `Forward2D*.shader` 系列，它们会写出 G-buffer 所需的四个通道；
4. 光源：变换 + Shape/Shaders/Textures2D + `Renderer::Color` + `Point` + `Gain` + `ShadowBuffer`，着色器为 `Light2DPoint.shader`（范围光、平行光对应 `Light2DRange` / `Light2DParallel`）——即编辑器“创建 → 2D光源”预设；
5. 遮挡物挂所需的阴影组件，编辑器“带2D阴影的基本渲染实体”预设已带 ShapeShadow、BlockShadow、SelfShadow 与 WallShadow。

```woolang
// 为世界装上 2D 延迟光照所需的系统（世界默认非激活，装好后再启用）
func setup_light_world(w: je::world)
{
    for (let name : [
        "Translation::TranslationUpdatingSystem",
        "Graphic::DeferLight2DGraphicPipelineSystem"])
        do w->add_system(je::typeinfo::load(name)->unwrap);
    w->set_enable(true);
}
```

⚠️ 注意 摄像机挂了 `CameraPostPass` 却没有 `Shaders` 时，场景会被画进 G-buffer，但光照与合成都不会执行，屏幕上什么也看不到。编辑器正是以“CameraPostPass 与 Shaders 同时存在”判定调试摄像机的 2D 光照模式是否开启。没有 `CameraPostPass` 的摄像机则直接绘制场景、不做任何光照。

### 脚本用法

#### Woolang（towoo）

```woolang
UPDATE!
{
    // 点光源逐渐熄灭：每秒把强度增益降低 0.5，最低为 0
    system! fade_out(gain: Light2D::Gain)
        contains Light2D::Point;
    {
        let g = gain.gain->get - je::deltatime() * 0.5;
        gain.gain->set(g < 0. ? 0. | g);
    }
}
```

#### C++（`jeecs::Light2D`，头文件 `jeecs/components.hpp`）

```cpp
void TorchSystem::Update()
{
    for (auto&& [gain, point] : query<view typesof(
        Light2D::Gain&, Light2D::Point&)>())
    {
        gain.gain = std::max(0.f, gain.gain - deltatime() * 0.5f);
        point.decay = 2.f;
    }
}
```

### 相关术语

参见[术语表](README.md)：

* [渲染](render.md)——Light2D 是渲染管线的一种，与 Unlit、UI 管线并列；
* [摄像机](camera.md)、[帧缓冲](framebuffer.md)——光照按摄像机执行，G-buffer、光照缓冲与阴影缓冲都是帧缓冲；
* [着色器](shader.md)——光源、物体与合成 pass 的效果都由着色器决定；
* [瓦片地图](tilemap.md)——瓦片分片使用 BlockShadow 的逐段排序与 PerBlockSelfShadow。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。实现主体位于 `src/jeecs_core_graphic_defer_light2d_system.cpp`，管线结构体见 `src/jeecs_core_graphic_system.hpp`，组件定义见 `include/jeecs/components.hpp` 的 `Light2D` 命名空间，着色器侧的 G-buffer 绑定约定见 `je/shader/light2d.wo`。

### 收集阶段（`GraphicUpdate`）

* **摄像机**：`Translation&` + `Projection&`，其余可选。带 `CameraPostPass` 时按（渲染目标 × 视口）尺寸懒创建/重建 `post_rend_target`——四个颜色附件（漫反射为 RGBA，自发光 / 视空间坐标 / 视空间法线为 16 位浮点 RGBA）加深度——以及按 `light_rend_ratio`（钳制到 [0, 1]）缩放的 `post_light_target`（16 位浮点 RGBA）；
* **普通渲染体**：与 Unlit 相同，但 `except` 了 `Point` / `Parallel` / `Range` / `Element`——光源与 UI 不作为普通物体绘制；
* **光源**：`Translation&` + `Shape&` + `Shaders&`，且 `anyof(Point, Parallel, Range)`。`ShadowBuffer` 的帧缓冲按**窗口尺寸** × `resolution_ratio` 懒创建（RGBA + 深度），尺寸变化时重建；`Range` 的光照网格在缺失时由各层轮廓点生成三角带（顶点 = 位置 + 强度），脚本写入 `shape` 会清掉旧网格触发重建；
* **遮挡物**：`Translation&` + `Shape&`，且挂有任一阴影组件。BlockShadow 网格懒生成（每个轮廓点一对顶点，挤出因子 0/1）；同时按形状局部 AABB 的 8 个角点算出世界包围盒的底边与顶边中点，供 TopDown 排序、高度判定与接触阴影复用。

### 每个摄像机的绘制流程（`DrawFrame`）

```
1. 阴影 pass      仅 CameraPostPass：逐光源（非平行光按视锥剔除）
                  若有 ShadowBuffer：清空 → 绘制范围内遮挡物的 Block/Shape/Wall/Self 阴影
                  → 最后统一绘制 SpriteShadow
2. G-buffer pass  有 CameraPostPass 时写 post_rend_target（自发光清 0、坐标清 zfar、法线清 (0,0,1)）
                  否则直接写摄像机目标；普通渲染体的绘制与 Unlit 相同
3. 光照 pass      需 CameraPostPass + Shaders：逐光源把 G-buffer（槽 4~7）与阴影（槽 8，
                  无阴影时为 1×1 占位纹理）绑定后，用光源着色器把 Shape/Range 网格加色画进光照缓冲
4. 合成 pass      全屏四边形画到摄像机真实目标：槽 0 = 光照结果，槽 4~7 = G-buffer
```

光照 pass 传入的 uniform：颜色（`Color` 线性值，A 乘以 `Gain`）、衰减（仅 Point / Range）、阴影缓冲分辨率。`CameraDefer2DPass.shader` 的合成公式为 `albedo × (自发光 + 光照 + 0.03)`，再做 `x / (x + 1)` 色调映射并乘以摄像机的 `Renderer::Color`。槽位起点在 C++ 侧为 `JE_LIGHT2D_DEFER_0 = 4`，与 `je/shader/light2d.wo` 中的同名常量及 `JE_LIGHT2D_Albedo` 等 uniform 一一对应。

### 阴影缓冲与逐像素仲裁

ShadowBuffer 的 R 通道存阴影强度（`BLEND(MAX)` 合并），深度附件存的不是相机深度，而是 *painter key*：普通光源取遮挡物世界 Z，TopDown 光源取包围盒底边世界 Y，每帧按全部遮挡物的 key 范围线性映射到 [0.05, 0.95]；阴影着色器以 `ZTEST(LESS_EQUAL)` 保留最靠前的贡献。于是“阴影盖住谁”由俯视角前后关系决定，而不是绘制先后：

* SelfShadow 的 key 比本实体前移半层（0.005），并以覆盖写入清除位于其后方的遮挡阴影——同图层的 BlockShadow 因此被清掉，消除自遮挡；
* SpriteShadow 只做 MAX 叠加、不写深度（其四边形只是椭圆与尾迹的包围框），并推迟到同一光源的其它贡献之后绘制，避免被后方接收面的覆盖写入擦掉；
* 瓦片地图分片的 BlockShadow 带内部标志 `vertex_priority`，在 TopDown 光源下改用顶点携带的逐段 key；`PerBlockSelfShadow` 则在顶点属性 3 中携带块底边，逐块判定受光。

阴影的投射原点为 `world_position + world_rotation * ShadowOffset::offset`；非平行光只处理距离不超过“遮挡物半径 + 光源半径”的遮挡物。

⚠️ 注意 每个光源只有**一块** ShadowBuffer，每个带 CameraPostPass 的摄像机都会在自己的视空间里把它清空并重画一次；其尺寸始终跟随窗口而非摄像机目标，渲染到较大帧缓冲的摄像机拿到的阴影分辨率仍按窗口计算。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 组件定义与成员注释 | `include/jeecs/components.hpp`（`Light2D`） |
| 延迟光照管线 | `src/jeecs_core_graphic_defer_light2d_system.cpp` |
| 管线结构体（shadow_pass_set / 各 arch / 资源） | `src/jeecs_core_graphic_system.hpp` |
| G-buffer 绑定约定 | `je/shader/light2d.wo` |
| 光源与合成着色器 | `builtin/shader/Light2D{Point,Range,Parallel}.shader`、`builtin/shader/CameraDefer2D*.shader` |
| 阴影着色器 | `builtin/shader/internal/defer_light2d_shadow_*.shader` |
| 编辑器预设 | `builtin/editor/Editor/ComponentPresets.wo`、`EntityClipboard.wo` |
