# REVIEW: support_linear_color_space

- 审查范围：`origin/main...HEAD`（10 个提交，50 个文件，约 +1779/−230）
- 审查方式：静态阅读 + Woolang 语法检查 + `joyecs` 增量编译
- 日期：2026-10-25

## 结论

方向正确、闭环完整：线性空间渲染 + sRGB 输出编码 + 纹理 sRGB 解码 +
编辑器颜色口径统一，四个后端和 Woolang 绑定均已覆盖，未发现阻塞性
正确性问题。

主要不优雅之处集中在：

1. sRGB 传输函数的重复实现；
2. `COLOR` 作为 uniform 存储类型造成的机械扩散；
3. 回退路径中屏幕尺寸/目标管理的重复与不可达分支。

## 已处理

- [x] 统一 sRGB 传输函数：`math::srgb_to_linear` / `math::linear_to_srgb`
      （`include/jeecs.hpp`），`math::color` 与 ImGui 顶点色线性化复用。
- [x] `GizmoDrawing.wo` 的 `linearColor` 改用 `color::from_srgb`，删除重复的
      `math::pow` 实现与 `import pkg::math`。
- [x] `WorldGrid*.shader`、`SelectedEntity.shader` 的预转换常量按决策维持原状。
- [x] `draw_action::set_uniform_color` 改走 color 专用的
      `wojeapi_draw_action_set_uniform_color`，由 `math::color::to_linear()`
      决定颜色到管线表示的转换；不再把四个分量直接当作最终 float4 写入。
- [x] `COLOR` 从 `uniform_type` 拆成 `uniform_mask` 位标签：颜色 uniform 的
      存储类型恢复为 `FLOAT4`，四个后端不再感知颜色语义；wrapper 从
      woshader `Type::Color` 写入标签，标签随 shader 缓存持久化
      （`SHADER_CACHE_VERSION` 已递增）。后续新增编辑器语义标签只需扩展
      `uniform_mask`。

## ImGui 线性颜色空间调查

- ImGui **没有**内建的线性颜色空间支持。`ImGuiConfigFlags_IsSRGB` 只是
  注释为 "Flags for general application use" 的保留位，ImGui 内部不读取、
  不做任何颜色转换。
- 官方后端的做法：GL/DX11/Metal/Vulkan 完全不做 gamma 处理，默认把
  `ImDrawVert.col`（8 位 sRGB）直接写入目标；只有 WebGPU 后端在目标是
  sRGB 格式时向 shader 传 `gamma = 2.2` 并做 `pow(color.rgb, gamma)`，
  属于近似处理。
- 因此当前分支在 CPU 端把顶点色 sRGB→linear、由 sRGB 目标统一编码的做法
  是正确且必要的；对不透明像素，最终结果与原始 sRGB 值一致。
- 副作用（可接受，但需知晓）：
  - 半透明/抗锯齿边缘在**线性空间**混合，观感与 ImGui 参考实现
    （gamma 空间混合）略有差异；
  - linear 值被量化回 8 位顶点色，暗部有精度损失；
  - 每帧每顶点 3 次 `pow`，可用 256 项 LUT 优化（可选）。
- 若要求像素级复刻 ImGui 观感，替代方案是 UI 在 sRGB 编码**之后**绘制
  （GL 可在 ImGui 绘制期间关闭 `FRAMEBUFFER_SRGB`），但这与当前“整帧统一
  编码”的架构冲突，不建议现在改。

## 待处理

### P1 类型安全（已确认，保持现状）

- [x] 所有颜色写入/转换必须经过 color 专用入口，不得假设 `color` 的内部
      存储布局。`wojeapi_color_to_linear`、`wojeapi_set_uniforms_color` 以及
      新增的 `wojeapi_draw_action_set_uniform_color` 都属于这类“语义入口”：
      即使当前实现是恒等转换或最终落到 float4 写入，也应保留，由 color
      类型自己决定表示为管线的形式。

### P2 设计/结构

- [x] `uniform_type::COLOR` 改为 `uniform_mask` 标签（存储仍为 FLOAT4）；
      backend layout switch、cmd、tilemap 序列化中的 COLOR 分支已删除，
      vertex-in / fragment-out 的 `COLOR→FLOAT4` 归一化也一并移除。
- [ ] GL 的屏幕尺寸解析在 `gl_commit_update` 与 `gl_begin_render_to_target`
      中重复，提取 helper；`jegui_commit_srgb_encode_pass` 里基于
      `source_texture` 尺寸的 `width <= 0` + 窗口尺寸 fallback 不可达，可删。
- [ ] `m_srgb_output_hardware` / `m_srgb_output_encode_pass` 改为单个 enum
      （HARDWARE / ENCODE_PASS），消除非法状态。
- [ ] VK `ensure_srgb_encode_screen_target` 失败后每帧 `logerr` 且不再重试，
      建议 latch（只记一次）或逐帧重试。
- [ ] 编码 pass 失败（shader 缺失/编译失败）时，GL/VK 都会把整帧留在离屏
      目标且不再写屏幕；建议失败后关闭重定向，保证至少能出图。

### P2 正确性/健壮性

- [ ] 桌面 GL 仅依赖 `GLFW_SRGB_CAPABLE` hint 就 enable
      `FRAMEBUFFER_SRGB`；建议用
      `glGetFramebufferAttachmentParameteriv(..., GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING)`
      验证，失败则切 encode pass。
- [ ] GL 的 encode 目标（FBO/RBO/纹理）在 `gl_shutdown` 未销毁，reboot 会
      泄漏；在 `gl_pre_shutdown` / `gl_shutdown` 补一行销毁。

### P2 导出

- [ ] `ExportProject.wo` 只排除 `*.shader.je4cache`，其余 `.je4cache` 全部
      打包，会带入 `imgui.ini.je4cache`、`history.je4cache` 等编辑器缓存；
      建议用 `isImagePath(purename(p))` 限定。`isImagePath` 目前位于
      `Browser.wo`，需移到共享位置。

### P3 小项

- [ ] `math::color` 的 4 行 defaulted copy/move/assign 可删。
- [ ] `_gl_extension_supported` 的 `name == nullptr`、
      `_jegui_linearize_draw_data` 的 `draw_data == nullptr` 等防御检查可删。
- [ ] `je_gui_coloredit4` / `je_gui_colorpicker4` 的参数解析与 ImGui flags 重复。
- [ ] `1 + TEXTURE + 1` 魔法映射（`src/jeecs_woolang_api.cpp`）改为 switch/常量。
- [ ] `FORMAT_MASK` 已无引用且语义现在包含 `SRGB`，删除或标注 legacy。
- [ ] `srgb_encode.shader` 的 `step(value, edge)` 与 GLSL 参数顺序相反，
      加注释防止后续误改。
- [ ] NDC scale 补丁在 DX11/Metal/VK 三处重复，可提取 helper。
- [ ] `builtin/script/editor.wo` 有 70 行格式化噪声（含 trailing whitespace）
      和无关的 quat `InputFloat3` 改动，建议拆分/回退。
- [ ] 动画编辑器颜色关键帧仍用 `InputFloat4` 编辑线性原始值
      （`Animation2D/DataModel.wo`），与其它颜色编辑器口径不一致。

## 兼容性提醒

1. 图片默认 sRGB 解码：已有项目中的法线图/遮罩图会被当作颜色贴图解码，
   需要用户手动标记为 Linear。
2. 已有组件颜色、旧 FLOAT4 材质 uniform 现在按线性值解释，场景观感会变化，
   且没有迁移机制；如为有意接受的破坏性变更，请记录到 changelog。
3. 8 位线性中间目标（GL/VK 回退路径、2D albedo G-buffer）在暗部会出现
   banding；如在意画质可改 RGBA16F。

## 本次验证

- `baozi check builtin/editor/Editor/WorldEditorViewport/GizmoDrawing.wo`：通过。
- `baozi check builtin/shader/CameraDefer2DFogPass.shader`（color uniform）：通过。
- `cmake --build build --config=RelWithDebInfo --target joyecs --parallel 8`：通过。
- 未运行 `jedriver`，未做实际画面验证。
