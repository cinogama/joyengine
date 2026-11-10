# Graphic render API v2

`je_graphic_cmd_*` 取代了旧的 `jegl_rchain_*` “录制-回放”层：它把绘制调用记录为显式命令流，
再在图形线程提交。新的 render API 仍然是“任意线程录制、图形线程提交”，但把绘制状态、绑定和 uniform
上传重新设计为显式、扁平、可去重的命令流，并与 GL 3.3 / GLES 3.0 / WebGL 2.0
的能力地板对齐。

## 设计原则

- **强类型**：`je_gl_pipeline_t`、`je_gl_sampler_t`、`je_gl_command_buffer_t` 是互不相同的
  opaque 类型；状态和格式使用 `enum class`；uniform 使用值类型 `je_gl_uniform_slot_t`，
  而不是裸偏移或 `uint32_t*`。
- **空安全**：必填指针在实现中用断言约束；确实可为空的参数在类型前标注
  `/* OPTIONAL */`，并写明空值语义。返回值同理。
- **不做防御式回退**：违反前置条件是未定义行为（Debug 构建断言终止），不会静默
  跳过、自动钳制或悄悄切换到旧路径。
- **复用资源层**：`je_gl_shader_t`、`je_gl_vertex_t`、`je_gl_texture_2d_t` 等纹理类型、`je_gl_frame_buffer_t`、
  `je_gl_uniform_buffer_t` 继续使用既有加载、引用计数、热重载和图形线程亲和性机制。

## 对象模型

| 对象 | 作用 |
|---|---|
| `je_gl_shader_t` | program + 反射（uniform 布局、纹理声明、默认采样器） |
| `je_gl_vertex_t` | 顶点/索引缓冲与顶点格式 |
| `je_gl_texture_2d_t/3d/array/cube` | 采样纹理 |
| `je_gl_frame_buffer_t` | 渲染目标与附件 |
| `je_gl_uniform_buffer_t` | 帧/材质级 UBO，绑定位置由创建时指定 |
| `je_gl_pipeline_t` | shader + 固定功能状态（拓扑/深度/混合/剔除/绕序/写掩码） |
| `je_gl_sampler_t` | 独立采样器，可覆盖 shader 默认采样器 |
| `je_gl_command_buffer_t` | 录制 pass/bind/uniform/draw 的命令缓冲区 |

`je_gl_pipeline_t` 持有 shader 的引用；命令缓冲区在录制期持有所有被引用资源的引用，
`je_graphic_cmd_reset` / `je_graphic_cmd_close` 时统一释放。

## 线程与生命周期

- 资源创建、pipeline/sampler 创建、命令录制可以在任意线程进行。
- `je_graphic_cmd_commit` 只允许在创建命令缓冲区时传入的 `je_gl_context_t*` 所属图形线程调用；
  实现会断言当前图形线程与 `m_context` 一致。
- `je_gl_pipeline_t` / `je_gl_sampler_t` 的后端对象在首次提交时惰性创建；关闭时通过现有
  的资源销毁队列在图形线程释放。
- shader 热重载后，pipeline 会在下一次提交时重新执行 `pipeline_init`。

## 录制规则

1. `je_graphic_cmd_reset` 后调用 `je_graphic_cmd_begin_pass`；一个命令缓冲区可以包含多个 pass。
2. 清屏命令只能在 `begin_pass` 之后、该 pass 的第一次 draw 之前调用。
3. draw 之前必须绑定 pipeline 和 vertex；否则断言失败。
4. `je_graphic_cmd_set_uniform_*` 只影响当前 pipeline 的 per-draw uniform 块；绑定新
   pipeline 会用 shader 的默认值重新初始化该块。
5. `je_graphic_cmd_commit` 会自动结束仍然打开的 pass；也可以显式调用
   `je_graphic_cmd_end_pass`。
6. commit 后命令缓冲区保留已录制内容；复用前调用 `je_graphic_cmd_reset`。

### 资源更新

资源内容分两类：

- **静态内容**：创建期快照（纹理像素 / 顶点数据）。后端在资源首次使用时上传；
  创建后快照只读。一致变量缓冲区没有可依赖的创建期内容：创建后不保证初始化。
- **本帧内容**：`je_graphic_cmd_update_vertex` / `je_graphic_cmd_update_uniform_buffer`
  把数据录制进命令缓冲，提交时由后端落实。更新**只对包含该命令的帧生效**，因此带
  动态内容的资源**必须**在绘制它的每一帧都录制一次。没有录制更新的帧，绑定到的内容
  **未定义**：取决于后端，可能是创建期内容，也可能是此前某次更新的内容，调用方不得
  依赖其中任何一种。
- **持久上传**：`je_graphic_cmd_upload_texture_*` 写入纹理自身的存储，提交后对后续帧
  持续可见（资产语义）。后端负责与在飞帧同步，属于稀有路径。

更新不需要处于 pass 内。推荐把更新录制在
`je_graphic_branch_new_update_command` 返回的**更新命令缓冲**中，并以最低优先级
（`INT32_MIN`）调用 `je_graphic_branch_new_frame`：宿主按优先级提交，更新先于本帧
所有绘制链落实。更新命令缓冲不开启 pass，窗口被跳过的帧中也会被提交（只跳过绘制）。

## 空安全约定

```cpp
JE_API void je_graphic_cmd_begin_pass(
    je_gl_command_buffer_t* cmd,
    /* OPTIONAL: nullptr 表示屏幕后备缓冲区 */ je_gl_frame_buffer_t* target,
    const je_gl_viewport_t* viewport);

JE_API /* OPTIONAL: m_offset == JE_GL_UNIFORM_SLOT_NONE 表示 shader 未声明 */
je_gl_uniform_slot_t je_graphic_pipeline_uniform_slot(
    je_gl_pipeline_t* pipeline, const char* name);
```

- 未标注 `OPTIONAL` 的指针参数不得为空；实现使用 `assert` 约束。
- `je_gl_uniform_slot_t` 是值类型，使用 `m_offset == JE_GL_UNIFORM_SLOT_NONE` 表示
  “该 shader 未声明此 uniform”。`je_graphic_cmd_set_uniform_*` 不接受无效 slot。

## Uniform 布局

`je_gl_shader_t` 在生成（或从缓存加载）时计算一次规范化 std140 布局：

- `je_gl_shader_uniform_variables_t::m_offset` / `m_size`：规范偏移和大小；
- `je_gl_shader_t::m_uniform_block_size`：整个 per-draw uniform 块的大小；
- `je_gl_builtin_uniform_slots_t`：pipeline 创建时缓存的 `JE_M` / `JE_MVP` 等位置。

为了让 HLSL cbuffer packing 与 std140 一致，shader 生成器按对齐等级对 uniform
排序（`je/shader/shader.wo` 的 `uniform_pack_rank`）。GL/DX11/VK/Metal 必须使用同一
份偏移；GL 后端在 Debug 构建中会把规范偏移与其自身计算结果做断言比对。

`JE_NDC_SCALE` 由后端按渲染目标自动管理，不通过 `je_gl_builtin_uniform_slots_t` 暴露。

## 示例

```cpp
je_gl_pipeline_desc_t pipeline_desc{};
pipeline_desc.m_shader = shader;      // 非空
pipeline_desc.m_state = nullptr;      // 使用 shader 的 ZTEST/ZWRITE/BLEND/CULL

je_gl_pipeline_t* pipeline = je_graphic_pipeline_create(context, &pipeline_desc);
const auto* slots = je_graphic_pipeline_builtin_uniform_slots(pipeline);

je_gl_command_buffer_t* cmd = je_graphic_cmd_create(context);
je_graphic_cmd_begin_pass(cmd, framebuffer, &viewport);

const float clear[4] = { 0.f, 0.f, 0.f, 1.f };
je_graphic_cmd_clear_color(cmd, 0, clear);
je_graphic_cmd_clear_depth(cmd, 1.f);

je_graphic_cmd_bind_pipeline(cmd, pipeline);
je_graphic_cmd_bind_vertex(cmd, vertex);

if (slots->m_model_view_projection.m_offset != JE_GL_UNIFORM_SLOT_NONE)
    je_graphic_cmd_set_uniform_float4x4(cmd, slots->m_model_view_projection, mvp);
if (slots->m_color.m_offset != JE_GL_UNIFORM_SLOT_NONE)
    je_graphic_cmd_set_uniform_float4(cmd, slots->m_color, 1.f, 1.f, 1.f, 1.f);

je_graphic_cmd_bind_texture_2d(cmd, 0, albedo);
je_graphic_cmd_draw_indexed(cmd, (uint32_t)vertex->m_index_count, 0, 1);

je_graphic_cmd_end_pass(cmd);
je_graphic_cmd_commit(cmd);   // 图形线程
je_graphic_cmd_reset(cmd);    // 复用
```

## 可移植性地板

新 API 只使用 OpenGL 3.3 / OpenGL ES 3.0 / WebGL 2.0 都具备的能力：

| 能力 | 实现方式 |
|---|---|
| 显式 pipeline | GL 状态缓存；DX11 输入布局/状态对象；VK/Metal 按目标缓存 PSO |
| per-draw uniform | 每帧 UBO ring + `glBindBufferRange`（WebGL2 用 `bufferSubData`） |
| 本帧顶点/UBO 内容 | 每帧瞬态 ring（Vulkan/Metal）或驱动丢弃语义（DX11 `WRITE_DISCARD` / GL orphan） |
| 独立 sampler | `glGenSamplers` / `glBindSampler` |
| instanced draw | `glDrawElementsInstanced` / `glDrawArraysInstanced` |
| MRT | `glDrawBuffers` |
| 纹理类型 | 2D / 3D / 2D array / cube |
| 索引 | `uint32_t` |

明确不使用：SSBO、compute、geometry/tessellation、indirect/multi-draw、base-vertex、
persistent mapping、无绑定纹理、顶点阶段纹理采样（v1 与 Metal 现状一致）。

## 与旧 API 的关系

- `jegl_rchain_*`、立即模式绘制 API（`jegl_bind_*` / `jegl_draw_vertex` /
  `jegl_rend_to_framebuffer` / `jegl_set_uniform_value`）及其 Woolang 绑定已删除。
- 资源加载/关闭接口（`je_graphic_texture_*` / `je_graphic_shader_*` /
  `je_graphic_vertex_*` / `je_graphic_framebuf_*` / `je_graphic_uniformbuf_*`）保留，
  作为新渲染 API 的资源层。
- 旧的“改 CPU 缓冲 + `m_modified` 懒上传”接口已删除：`je_graphic_vertex_buffer_update`、
  `je_graphic_uniformbuf_update`、纹理的 `clip` / `pix` / `pixel` / `mark_modified` /
  快照接口全部移除。修改资源只能通过命令化的更新/上传接口。
- 分支类型重命名为 `je_render_branch_t`，只承载 command buffer。

## 当前实现状态

| 后端 | render vtable 状态 |
|---|---|
| OpenGL 3.3 / GLES 3.0 / WebGL 2.0 | 已实现（pass/pipeline/sampler/UBO ring/instanced draw） |
| none（`-gapi none`） | 已实现（全部 no-op） |
| DirectX 11 | 已实现（pass/pipeline/sampler/uniform/draw） |
| Vulkan 1.2 | 已实现（frame-in-flight、每帧 UBO ring、PSO/pipeline 状态缓存、内存子分配、屏幕 MSAA resolve） |
| Metal | 已实现（延迟编码器；Windows 构建不编译 Metal 代码，待 macOS 构建验证） |

## Vulkan 1.2 后端说明

- 帧模型：`FRAMES_IN_FLIGHT = 2`，每帧独立的命令缓冲、fence、信号量和 descriptor pool；
  同一帧的所有 pass 录制进一个命令缓冲，在 `update_draw_commit` 阶段一次提交。
  `begin_pass` / `end_pass` 不再触发 queue submit。
- per-draw uniform：每帧一个持久映射的 HOST_VISIBLE|HOST_COHERENT ring，set 0 使用
  `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC` 和 dynamic offset；不再按 draw 分配 UBO。
- 本帧内容（顶点 / 用户 UBO）：同一个每帧瞬态 ring（16MB，按帧槽位独占、槽位复用
  前等待 fence）线性分配。更新写 ring 切片并记录帧序号，绑定阶段按序号改绑
  ring+offset；创建期内容保留在资源自己的持久缓冲里，供静态资源使用（未录制更新的
  帧也会绑定到它，这是本后端的实现细节，不构成接口保证）。
  ring 按帧槽位隔离，在飞帧永远不会读到别的帧写入的数据。
- 持久纹理上传：像素经 ring 暂存后在帧命令缓冲里 `vkCmdCopyBufferToImage`；提交前
  先等待另一个在飞帧槽位完成（稀有路径）。初始化上传走一次临时提交并等待完成；
  ring 剩余空间放不下的区段、以及处在 render pass 内的上传同样退化为临时提交。
- 固定功能状态：PSO 按 `(pipeline, framebuffer, topology, stride, sample count)` 缓存，
  状态来自 `je_gl_pipeline_t::m_state`（深度/混合/剔除/绕序/颜色写掩码），并受
  `VK_EXT_`/设备 feature 支持情况约束。
- 内存：buffer/image 使用大块 `VkDeviceMemory` 子分配；资源销毁时归还范围。
- MSAA：屏幕目标按 `je_gl_interface_config_t::m_msaa` 选择设备支持的 sample count，
  通过 render pass 的 resolve attachment 输出到交换链；`je_gl_texture_msaa_t` 仅作为
  render target 附件使用，不能通过普通纹理路径采样。
- 交换链重建时复用 render pass 缓存；render pass 兼容时保留 ImGui 上下文，只更新
  `MinImageCount`。
- 已知限制：`texture_msaa` 不支持 CPU 像素更新；ImGui 的 `apply_shader_sampler`
  回调当前为空（自定义 sampler 合并在纹理描述符中创建）；`shader_update` /
  `framebuffer_update` 与 DX11 现状一致，热重载依赖资源重新创建。
- 验证层只在 Debug 构建启用；Release/RelWithDebInfo 不加载 `VK_LAYER_KHRONOS_validation`。
- 设备选择优先独显；示例版本从 loader 报告的版本开始，遇到
  `VK_ERROR_INCOMPATIBLE_DRIVER` 会回退到 Vulkan 1.0。

所有内置后端都实现了 render API；自定义后端缺少该表时会在图形线程启动阶段报错。

## 验证

```shell
# 在可执行文件旁准备 builtin/ je/ pkg/ 资源后：
jedriver.exe -test-shader-layout
```

该自测会：

1. 加载内置 `builtin/shader/Unlit.shader`；
2. 校验 uniform 的规范 std140 偏移、排序和 `JE_*` 内建位置；
3. 校验纹理反射（名称、pass、sampler id、kind）；
4. 创建 pipeline/sampler/vertex/texture，录制一个完整的 draw（不提交），
   验证资源引用保持与释放。

输出以 `[shader-layout-test] ... OK` 和 `[render-recording-test] OK` 结尾。

