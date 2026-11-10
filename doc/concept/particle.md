# 粒子（Particle）

本文档是[术语表](README.md)中「粒子」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述粒子发射器的行为约定与用法；
* **第二部分·底层实现**——面向引擎维护者，描述上述行为在引擎内部是如何实现的。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 粒子是什么

粒子（Particle）用于模拟火焰、烟雾、火花等大量细小对象的产生、运动与消亡。一个*发射器*实体挂 `Particle::Emitter` 组件来描述“怎样发射、粒子怎样运动与变化”；粒子系统 `Particle::ParticleSystem` 在 CPU 上逐帧模拟，把存活的粒子构建成一张始终面向摄像机的*公告板*网格，写进发射器实体自己的 `Renderer::Shape`，再由常规渲染管线随实体一起绘制。

因此粒子没有单独的渲染管线：它就是一个网格每帧都在变化的普通渲染实体。

### 发射器需要哪些组件

| 组件 | 作用 |
| --- | --- |
| 变换组件（含 `Transform::Translation`） | 发射位置与朝向；绘制也需要它 |
| `Particle::Emitter` | 发射与生命周期参数 |
| `Renderer::Shape` | 粒子网格的承载者，由系统写入；**缺少它的发射器不会被模拟** |
| `Renderer::Shaders` | 粒子着色器（见下表） |
| `Renderer::Textures2D` | 粒子纹理绑定在 pass 0，未绑定时使用默认纹理 |
| `Renderer::Rendqueue`（可选） | 绘制顺序（编辑器预设会带上它） |

内置粒子着色器：

| 着色器 | 适用 |
| --- | --- |
| `ParticleBlend.shader` | 常规 alpha 混合（烟、尘、雪） |
| `ParticleAdd.shader` | 加色混合（发光火花、余烬） |
| `Forward2DParticle.shader` | 2D 延迟光照管线，接受光照与阴影（见[光照](light.md)） |
| `Forward2DParticleSelfGlowing.shader` | 2D 延迟光照管线，写入自发光通道，不受光照衰减影响 |

编辑器“创建 → 粒子发射器”预设会一次挂齐上述组件并指定 `ParticleBlend.shader`，推荐从它开始。

⚠️ 注意 `Emitter` 的头文件注释称“系统会在缺失时自动补齐并挂上内置粒子着色器”，但当前实现**没有**这项逻辑：系统不会添加任何组件。编辑器常用组件组合中的“粒子发射器”只包含 `Emitter` 一个组件，单独使用时什么也看不到——请改用“创建 → 粒子发射器”实体预设，或自行挂齐。

### Emitter 参数

区间类参数在 [min, max] 内均匀随机（两端写反也可以），角度类参数以**度**为单位。

| 分组 | 成员（括号内为默认值） | 说明 |
| --- | --- | --- |
| 发射 | `shape`（point） | 发射区域：`point` / `sphere`（体积内）/ `circle`（局部 XY 平面圆盘）/ `box` |
| | `shape_radius`（0.25）、`shape_size`（0.5, 0.5, 0.5） | 球/圆的半径；盒的**半**边长 |
| | `spawn_rate`（32） | 每秒连续发射数量 |
| | `burst_count`（0）、`burst_interval`（1） | 每隔 interval 秒爆发 count 个；count 为 0 时关闭 |
| 初速与受力 | `lifetime_min/max`（2 ~ 3） | 寿命（秒） |
| | `speed_min/max`（0.8 ~ 1.6）、`direction`（0, 1, 0） | 初速大小与主方向 |
| | `spread_angle`（25） | 围绕主方向的散布半角，180 为全球面 |
| | `gravity`（0, -0.8, 0）、`drag`（0） | 加速度；速度每秒按 e^(-drag) 指数衰减 |
| 初始外观 | `size_min/max`（0.08 ~ 0.16） | 尺寸（世界单位） |
| | `color_min/max`（白） | 在两色之间随机取一个插值点 |
| | `rotation_min/max`、`angular_min/max`（0） | 初始自转（度）与自转速度（度/秒） |
| 生命周期调制 | `color_over_lifetime` | 颜色渐变：`m_times` 与 `m_colors` 一一对应的关键帧（升序），默认由不透明白渐变到透明白 |
| | `size_over_lifetime` | 标量曲线：`m_keys` 为 (时间, 数值) 关键帧，默认恒为 1 |
| 通用 | `max_particles`（512） | 粒子数上限（最多 65536） |
| | `simulation_space`（world） | 模拟空间，见下节 |
| | `playing`（true）、`clear_particles`（false） | 播放开关与清空标记 |

生命周期调制以粒子的归一化年龄 t = 已存活时间 / 寿命（0 ~ 1）采样，结果**乘**到初始值上：颜色 = 初始色 × 渐变色，尺寸 = 初始尺寸 × 曲线值。关键帧之间线性插值，t 超出范围时取首/末关键帧。

### 世界空间与局部空间

* `world`：粒子位置存于世界坐标。发射器移动时，已发射的粒子留在原地——适合拖尾、烟迹；
* `local`：粒子位置存于发射器的局部坐标，整体随发射器一起移动、旋转、缩放——适合附着在角色身上的光环；此时 `gravity` 按发射器的局部坐标系解释。

### 播放控制

* `playing = false` **冻结整个模拟**：既不发射，也不推进已有粒子的年龄与运动，粒子网格保持上一次构建的状态；
* 把 `clear_particles` 置为 true，系统会在下一次模拟时清空全部已发射粒子，并把标记复位为 false；
* 修改 `max_particles` 会重建粒子池，已有粒子随之清空。

编辑器新建世界时会注册 `Particle::ParticleSystem`，因此编辑状态下即可在视口中实时预览、调参。

### 脚本用法

#### Woolang（towoo）

```woolang
import je::input;   // 其余导入同 towoo 系统模板

UPDATE!
{
    // 按住 R：清空已发射的粒子；按住 P：冻结整个粒子模拟
    system! fx_control(emitter: Particle::Emitter)
    {
        if (je::input::keydown(je::keycode::R))
            emitter.clear_particles->set(true);
        emitter.playing->set(!je::input::keydown(je::keycode::P));
    }
}
```

#### C++（`jeecs::Particle::Emitter`，头文件 `jeecs/components.hpp`）

C++ 成员带 `m_` 前缀（脚本与编辑器中的名字去掉前缀）：

```cpp
void FxSystem::Update()
{
    for (auto&& [emitter] : query<view typesof(Particle::Emitter&)>())
    {
        emitter.m_spawn_rate = 0.f;         // 关闭连续发射，只保留周期爆发
        emitter.m_burst_count = 20;
        emitter.m_burst_interval = 0.5f;
        emitter.m_simulation_space =
            Particle::Emitter::simulation_space_type::local;
    }
}
```

### 粒子烘焙器

`je/editor/particalbaker.wo`（文件名拼写如此）是与 Emitter / 粒子系统**无关**的另一套机制：它把用户脚本描述的粒子动画逐像素“画”进一组纹理，得到可循环播放的*序列帧*。用户脚本提供 `launcher`（在其中调用 `launch(起始帧, 数量)` 登记发射批次）、`perparticle`（为每个粒子生成自定义数据）、`position`（返回某帧要绘制的若干位置，坐标范围 -1 ~ 1）与 `color` 四个函数；烘焙参数 `frame_count`（帧数）、`frame_size`（每帧像素尺寸）、`frame_rate`（每帧秒数，即脚本中 `deltatime()` 的返回值）与 `repeat_round`（模拟轮数）。每个粒子被模拟 `frame_count × repeat_round` 帧，帧号对 `frame_count` 取模写入，所以序列首尾自然衔接；结果存于 `m_frames_caches`。目前仓库中没有其它代码调用它的入口 `_start_baking`。

### 相关术语

参见[术语表](README.md)：

* [渲染](render.md)、[着色器](shader.md)——粒子由常规管线绘制，外观与混合方式由着色器决定；
* [网格](mesh.md)——粒子系统每帧重写发射器的网格；
* [纹理](texture.md)——粒子纹理绑定在 pass 0；
* [光照](light.md)——配合 Forward2DParticle 系列着色器可以接受 2D 光照；
* [差异时间](deltatime.md)——模拟按系统的差异时间推进。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。实现位于 `src/jeecs_core_particle_system.hpp/.cpp`，组件定义与参数注释见 `include/jeecs/components.hpp` 的 `Particle` 命名空间，公告板展开见 `builtin/shader/ParticleBlend.shader` 等内置粒子着色器。

### 每帧流程

`ParticleSystem` 声明的是 `GraphicUpdate()`，即每个世界内的最后一个更新阶段（`m_graphic_update`），与各渲染管线系统同阶段。查询为 `Particle::Emitter&` + `Translation*` + `Shape&`，对每个发射器：

1. **池准备**：取该发射器的 `particle_pool`（SoA 数组：位置、速度、年龄、寿命、尺寸、自转、颜色、粒子 ID）。容量 = `max_particles` 钳制到 [1, 65536]；容量变化或尚无 GPU 网格时重建池，并一次性创建容量大小的顶点缓冲——索引按每四边形 `0,1,2, 2,1,3` 静态生成——随后把它写进 `Shape::vertex`；
2. **清空**：`m_clear_particles` 为真时重置池（存活数、发射累加器、爆发计时器）并复位标记；
3. **模拟**（仅 `m_playing`）：发射累加器 += `spawn_rate × dt`，取整数部分发射；爆发计时器按 `burst_interval` 循环触发；随后逐粒子积分——年龄 += dt，到寿命即与末尾粒子交换删除；速度 = 速度 × e^(-drag·dt) + 重力 × dt，位置 += 速度 × dt（半隐式欧拉），自转角 += 自转速度 × dt；最后标记 dirty；
4. **构建网格**（仅 dirty）：世界空间模拟时先用发射器的逆变换把粒子位置转回局部坐标；按年龄比例采样颜色渐变与尺寸曲线；四个角点在 CPU 上完成自转与缩放；包围盒外扩最大粒子尺寸后写入顶点数据的边界（供视锥剔除）；整块暂存数据经一次缓冲更新上传，激活索引数收缩为 存活数 × 6。

最后回收已不存在的发射器所对应的池。

### 发射细节（`emit_one`）

* 发射偏移：`sphere` 用随机方向 × 半径 × ∛rand 得到体积内均匀分布；`circle` 用 半径 × √rand 得到圆盘内均匀分布；`box` 在 ±`shape_size` 内逐轴均匀；
* 初速方向：以“发射器世界旋转 × `direction`”为轴，在半角 `spread_angle` 的圆锥内按余弦均匀采样，≥ 180° 退化为均匀球面；
* 世界空间下偏移先乘发射器的 `local_scale`、再经世界旋转与位置变换；局部空间下偏移与速度直接存于局部坐标；
* 初始颜色在线性空间中以同一个随机系数对 `color_min` / `color_max` 的四个通道插值；
* 粒子 ID 单调递增，在 INT32 最大值处回绕为 0。

### 顶点格式与着色器

每顶点 12 个 32 位分量：中心位置（3，发射器局部坐标）+ 颜色（4）+ 已自转缩放的角点偏移（2）+ UV（2）+ 粒子 ID（1，`INT32`，以位模式存于 float 暂存区）。角点取 ±1 乘以尺寸，因此公告板四边形的边长为 2 × size。

着色器先用实体的模型矩阵把中心变换到世界空间，再沿视图矩阵行向量（世界空间的摄像机 right / up）展开角点——CPU 只做模拟，同一份网格对任意数量的摄像机都正确朝向；角点在模型矩阵之后叠加，所以粒子尺寸不受发射器缩放影响。

⚠️ 注意 以下几点以代码为准，与注释或直觉存在出入：

* 头文件注释写“运行相位：Update（在物理与渲染之前完成数据构建）”，实际声明与注册的是 `GraphicUpdate`；
* 查询中 `Translation` 为可选，但 `emit_one` 无条件读取其世界旋转——发射器必须带变换组件；
* `emit_one` 在两种模拟空间下都先用发射器的世界旋转变换 `direction`，而局部空间的粒子随后还会经模型矩阵绘制，发射器带旋转时发射方向会被旋转两次，与“局部空间下 direction 按局部坐标系解释”的说法不符；
* 粒子池以 `e._m_raw._m_id`（实体在所属 chunk 内的槽位号）为键、不含 chunk 信息：位于不同 chunk（如组件构成不同）而槽位号相同的发射器会共用一个池，碎片整理搬动实体后池也可能与实体错配。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| Emitter、color_gradient、scalar_curve 定义与注释 | `include/jeecs/components.hpp`（`Particle`） |
| 粒子系统（池、发射、积分、网格构建） | `src/jeecs_core_particle_system.hpp/.cpp` |
| 内置粒子着色器 | `builtin/shader/{ParticleBlend,ParticleAdd,Forward2DParticle,Forward2DParticleSelfGlowing}.shader` |
| towoo 组件声明 | `je/towoo/components.wo`（`Particle::Emitter`） |
| 粒子烘焙器 | `je/editor/particalbaker.wo` |
| 编辑器预设与默认系统 | `builtin/editor/Editor/EntityClipboard.wo`、`ComponentPresets.wo`、`WorldEditorViewport/WorldOpen.wo` |
