# 注释/文档与实现不符清单

本清单收录 2026-11 概念文档（`doc/concept/`）撰写过程中发现的“注释或文档描述与实际实现不一致”的点。这些不是功能错误（功能错误见 [BUGS.md](BUGS.md)），而是描述失真——按注释或旧文档去理解代码会得出错误结论。

## 引擎注释

### DIFF-01 入口查找顺序的注释写反

* 位置：`include/jeecs/core.hpp` 注释、`doc/capi/core/je_core_main_script_entry.md`
* 注释说法：先查编辑器入口、后查游戏入口
* 实际实现（`src/jeecs_startup.cpp`）：**先**找 `@/builtin/main.wo`（游戏入口），**找不到才**回退编辑器
* 参考：`doc/concept/entry.md`

### DIFF-02 `game.hpp` 对失效实体句柄的承诺不成立

* 位置：`include/jeecs/game.hpp` 注释称失效实体句柄的“操作不生效”
* 实际实现：碎片整理会直接 `delete` 搬空的 chunk，跨帧句柄成为**悬垂指针**，操作属未定义行为
* 参考：`doc/concept/entity.md`；对应危害见 [BUGS.md](BUGS.md) BUG-07

### DIFF-03 `Particle::Emitter` 头文件注释三处失实

* 位置：`include/jeecs/components.hpp`，`Particle::Emitter` 注释块
1. 注释称“系统会在缺失时自动补齐三件套并挂上内置粒子着色器”——代码中**无此逻辑**；缺少 `Renderer::Shape` 的发射器根本不参与模拟，且编辑器“粒子发射器”组合预设也只挂 `Emitter` 一个组件；
2. 注释称系统运行在 "Update" 阶段——实际实现的是 `GraphicUpdate`；
3. 注释称 `local` 模拟空间下 `m_direction` “按发射器局部坐标系解释”——实际发射方向被旋转两次（疑似实现错误，见 [BUGS.md](BUGS.md) BUG-23）
* 参考：`doc/concept/particle.md`

### DIFF-04 UI 文本“无需 Renderer 组件”的说法与渲染查询不符

* 位置：`UserInterface::Text` 组件注释、编辑器“UI文本”组合预设
* 注释/预设说法：UI 文本无需任何 `Renderer` 组件
* 实际实现：UI 渲染查询把 `Renderer::Shaders` 设为**必需**组件，缺少 `Shaders` 的文本实体不会被绘制
* 参考：`doc/concept/ui.md`

### DIFF-05 `je_graphic_vertex_load` 头注释的顶点布局漏写切线

* 位置：`include/jeecs/graphic.hpp`
* 注释说法：顶点布局为 位置 3 + UV 2 + 法线 3 + 骨骼 4 + 权重 4
* 实际实现：还有**切线 3** 分量（每顶点共 76 字节）
* 参考：`doc/concept/mesh.md`

### DIFF-06 `je/cache.wo` 注释称失败时“调用方会删除半成品缓存”

* 位置：`je/cache.wo`
* 实际实现：创建失败时**并不删除**半成品文件（失败的缓存文件只是永远通不过校验）
* 参考：`doc/concept/cache.md`

### DIFF-07 术语表颜色取值范围描述（已修正）

* 原说法：颜色各分量取值范围为 0~1
* 实际实现：按线性空间存储，RGB 分量允许超过 1 以表示 HDR
* 状态：`doc/concept/README.md` 已于本次更新修正；留档备查
* 参考：`doc/concept/math.md`

## 项目文档

### DIFF-08 AGENTS.md 称 prebuild 执行 “shader-wrap generation”

* 位置：`AGENTS.md`（`script/prebuild/` 一节，"shader-wrap generation etc."）
* 实际实现：着色器 wrap 在**着色器首次加载时**由 `_jegl_load_shader_source_impl` 生成；`script/prebuild/prebuild.wo` 完全不处理着色器
* 参考：`doc/concept/shader.md`

### DIFF-09 `SystemList.wo` 头注释“导出功能暂未支持”已过时

* 位置：`builtin/editor/Editor/SystemList.wo`
* 实际实现：编辑器已有完整的导出流程
* 参考：`doc/concept/editor.md`

### DIFF-10 `Practice::setHistorySuppressed` 从未被调用

* 位置：编辑器代码
* 实际实现：该函数无任何调用点，属遗留死代码（编辑历史抑制的实际开关在别处）
* 参考：`doc/concept/editor.md`

### DIFF-11 `je/editor/graphic.wo` 与 `je/editor/particalbaker.wo` 无任何引用

* 位置：`je/editor/` 下两个文件
* 实际实现：编辑器源码中没有任何地方引用它们；`particalbaker` 与粒子 `Emitter` 无关，也没有调用入口
* 建议：确认用途后清理，或在文件头注明状态
* 参考：`doc/concept/editor.md`

### DIFF-12 编辑器对“正交摄像机”的判定与渲染系统不一致

* 位置：渲染系统 vs 编辑器（`builtin/editor`）
* 不一致点：渲染按“挂 `OrthoProjection` 即正交”处理；编辑器按“没有 `PerspectiveProjection` 才算正交”处理
* 说明：双投影组件并存时两侧行为分叉，属实现内部不一致，后果见 [BUGS.md](BUGS.md) BUG-22
* 参考：`doc/concept/camera.md`
