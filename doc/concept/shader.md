# 着色器（Shader）

本文档是[术语表](README.md)中「着色器」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述着色器文件的写法、uniform 的读写以及着色器如何参与实体渲染；
* **第二部分·底层实现**——面向引擎维护者，描述着色器在引擎内部如何被编译到各图形后端、uniform 如何布局与写入。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 着色器是什么

着色器（Shader）描述[网格](mesh.md)的顶点如何被变换到屏幕上，以及每个像素如何着色。JoyEngine **没有独立的“材质”概念**：采样器的过滤与寻址方式、深度测试与写入、混合模式、面剔除都写在着色器里；一个实体的“材质”就是 `Renderer::Shaders`（着色器列表）、`Renderer::Textures2D`（[纹理](texture.md)通道）与 `Renderer::Color`（颜色）等组件的组合，编辑器的“材质”面板编辑的正是模型、着色器列表与纹理通道。

着色器源文件（`.shader`）是一段 Woolang 脚本，用 `pkg::woshader` 提供的着色器 DSL 编写。引擎在加载时把它翻译为各图形后端（DX11 / OpenGL / OpenGL ES / WebGL / Vulkan / Metal）的着色语言，**同一份源码在所有后端上通用**。

### 着色器文件的结构

以内置的 `Unlit.shader` 为例（排版略有压缩）。所有着色器都必须 `import je::shader`：它提供渲染状态常量、内置 uniform 与 `uvtrans` 等函数，也是引擎生成着色器代码的入口。

```woolang
import pkg::std;
import je::shader;
import pkg::woshader;
using woshader;
using je::shader;

SHARED(true); ZTEST(LESS); ZWRITE(ENABLE); CULL(NONE);

WOSHADER_VERTEX_IN!
    using vin = struct { vertex : float3, uv : float2, };
WOSHADER_VERTEX_TO_FRAGMENT!
    using v2f = struct { pos : float4, uv : float2, };
WOSHADER_FRAGMENT_OUT!
    using fout = struct { color : float4, };

public func vert(v: vin)
{
    return v2f{
        pos = JE_MVP * vec4!(v.vertex, 1.),
        uv = uvtrans(v.uv, JE_UV_TILING, JE_UV_OFFSET),
    };
}
let NearestSampler = Sampler2D::create(NEAREST, NEAREST, NEAREST, CLAMP, CLAMP);
WOSHADER_UNIFORM!
    let Main = texture2d::uniform(0, NearestSampler);
public func frag(vf: v2f)
{
    return fout{ color = alphatest(JE_COLOR * tex2d(Main, vf.uv)), };
}
```

* **渲染状态**：`SHARED`（是否按路径共享，见[加载与共享](#加载与共享)）、`ZTEST`（深度测试函数）、`ZWRITE`（深度写入）、`BLEND(方程, 源因子, 目标因子)`、`CULL`（面剔除）。缺省为不共享、`LESS`、`ENABLE`、不混合、不剔除；
* **`WOSHADER_VERTEX_IN!`**：顶点输入结构，字段**按顺序**对应网格的顶点属性；网格可以提供比着色器所需更多的属性；
* **`WOSHADER_VERTEX_TO_FRAGMENT!`**：顶点到片元的插值结构，**第一个字段是裁剪空间位置**；
* **`WOSHADER_FRAGMENT_OUT!`**：片元输出，每个字段对应一个渲染目标附件（2D 光照管线的前向着色器输出 4 个）；`vert` / `frag` 分别是顶点与片元着色函数；
* **`WOSHADER_UNIFORM!`**：声明 uniform 及默认值，如 `vec1!(1.)`（float）、`color::uniform(r, g, b, a)`（颜色，按 sRGB 填写，声明时转为线性）、`texture2d::uniform(通道, 采样器)`（纹理，另有 `texture2darray` / `texture3d` / `texturecube` / `texture2dms`）；
* **采样器**：`Sampler2D::create(缩小, 放大, mip, U 寻址, V 寻址)`，过滤取 `NEAREST` / `LINEAR`，寻址取 `CLAMP` / `REPEAT`；`WOSHADER_FUNCTION!` 可声明复用的着色函数。

### 引擎提供的内置 uniform

| 名称 | 类型 | 含义 |
| --- | --- | --- |
| `JE_M` / `JE_MV` / `JE_MVP` | float4x4 | 模型、模型-视图、模型-视图-投影矩阵 |
| `JE_LOCAL_SCALE` | float3 | 实体的局部缩放 |
| `JE_UV_TILING` / `JE_UV_OFFSET` | float2 | `Renderer::Textures2D` 的 tiling / offset |
| `JE_COLOR` | float4 | `Renderer::Color` 的颜色（线性；没有该组件时为白色） |
| `JE_V` / `JE_P` / `JE_VP` | float4x4 | 摄像机的视图、投影、视图-投影矩阵 |
| `JE_TIME` | float4 | x 为当前时间，y / z / w 为周期 0.5 / 1 / 2 秒的三角波 |
| `JE_LIGHT2D_*`、`JE_SHADOW_*` | — | 2D 光照管线专用（见[光照](light.md)） |

* 渲染系统在每次绘制前写入这些值；**只有着色器实际用到的 uniform 会保留**在生成的代码中；
* **以 `JE_` 开头的名字保留给引擎**：编辑器材质面板不展示、场景保存不记录它们，自定义 uniform 不要使用这个前缀。

### 加载与共享

`shader::load(universe, path)` 从 `.shader` 文件加载，失败（文件不存在、编译或生成失败）时返回 `none`。首次加载会编译并生成各后端代码，写入 `<路径>.je4cache`，源码不变时之后直接读取缓存（见[缓存](cache.md)）。`shader::create(universe, vpath, src)` 则从内存中的源码字符串创建，`vpath` 是这份着色器的虚拟路径，作为共享与缓存的键，缓存按源码内容校验。

**共享规则**：声明 `SHARED(true)` 的着色器在传入宇宙加载时按路径共享——同一路径得到**同一个实例**，修改它的 uniform 会影响所有使用者；`SHARED(false)` 的着色器每次加载都得到独立实例，可以为不同实体设置不同的参数。`builtin/shader/` 下带可调参数的着色器（如 `Forward2DSelfGlowing`、`CameraDefer2DFogPass`）都声明为 `SHARED(false)`。

### Uniform 的读写

* `shader::set_uniform(shad, name, val)` 写入 `int`、`real` 及其 2~4 元组；颜色 uniform 用 `shader::set_uniform_color(shad, name, rgba)`（线性 RGBA）；名字不存在时静默无效果，类型不符时记录错误、不写入；
* `shader::get_uniforms(shad)` 返回 `map<string, uniform_variable>`，取值为 `integer` … `float4`、`color`、`texture(通道号)` 或 `others`（矩阵等）；
* 写入的是**着色器资源上的值，它是之后每次使用该资源绘制时的初始值**——结合共享规则理解：共享实例上的修改是全局的。逐次绘制才变化的值（包括矩阵）只能在自定义渲染中通过 `bound_shader` 设置（见[渲染](render.md)）。

### 着色器与实体：多 pass 绘制

* `Renderer::Shaders` 持有一个着色器列表，渲染系统对实体的网格**按列表顺序、每个着色器各绘制一次**——每个着色器就是一个绘制 pass，各自使用自己的渲染状态；常规渲染管线要求实体至少带有 `Transform::Translation`、`Renderer::Shape` 与 `Renderer::Shaders`；
* 所有 pass 共用同一个网格与同一组纹理绑定，每个着色器按自己声明的通道号取用纹理——纹理通道与 pass **不是一一对应**的；
* 列表为空时使用引擎默认着色器（`!/builtin/shader/internal/builtin_default.shader`，输出在黑色与品红之间闪烁的颜色），提示“没有设置着色器”；
* Woolang 的 `entity::set_shaders(e, shaders)` 整体替换列表，`entity::get_shaders(e)` 读取；**实体须已挂有 `Renderer::Shaders`**，否则静默无效果。

### 内置着色器

内置着色器位于 `!/builtin/shader/`：

* 无光照：`Unlit`、`UnlitBlend`（混合）、`Mono`（纯色，输出 `JE_COLOR`）；
* 2D 光照前向：`Forward2D`、`Forward2DNormal`、`Forward2DMono`、`Forward2DAura`、`Forward2DParticle` 及各 `*SelfGlowing` 变体；
* 2D 光源与后处理：`Light2DPoint`、`Light2DParallel`、`Light2DRange`、`CameraDefer2DPass`、`CameraDefer2DBloomPass`、`CameraDefer2DFogPass`；
* 粒子 / 瓦片地图 / UI：`ParticleAdd`、`ParticleBlend`；`TilemapForward2D`、`TilemapForward2DNormal`、`TilemapUnlitBlend`；`UIText`；
* 引擎内部（`internal/`）：默认着色器、2D 光照阴影 pass、ImGui 图像显示、sRGB 编码。

`je::shader::light2d`（`je/shader/light2d.wo`）为 2D 光照着色器声明了 G-buffer 纹理（通道 4~8：Albedo、SelfLuminescence、VSpacePosition、VSpaceNormalize、Shadow）与 `JE_LIGHT2D_RESOLUTION` / `JE_LIGHT2D_DECAY`。

### API 速览

#### Woolang（`je::graphic`）

| 函数 | 说明 |
| --- | --- |
| `shader::load(universe, path)` / `shader::create(universe, vpath, src)` | 从文件 / 源码字符串创建，失败返回 `none` |
| `shader::path(shad)` / `shader::get_uniforms(shad)` | 资源路径；全部 uniform 及当前值 |
| `shader::set_uniform(shad, name, val)` / `shader::set_uniform_color(...)` | 写 uniform |
| `entity::set_shaders(e, shaders)` / `entity::get_shaders(e)` | 实体的着色器列表 |

```woolang
import pkg::std;
import je;
import je::ecs;
import je::graphic;

using je;
using je::graphic;

func setup_shader(e: entity, univ: universe)
{
    match (shader::load(option::value(univ), "!/builtin/shader/Forward2DSelfGlowing.shader"))
    {
        value(shad)?
        {
            shad->set_uniform("SelfGlowing", 2.);
            e->set_shaders([shad]); // 实体须已挂 Renderer::Shaders
        }
        none? je::logerr("着色器加载失败");
    }
}
```

#### C++（`jeecs::graphic::shader`，头文件 `jeecs/graphic.hpp`）

| 方法 | 说明 |
| --- | --- |
| `shader::load(context, path)` / `shader::create(context, vpath, src)` | 返回 `std::optional`；context 为 nullptr 时不参与共享 |
| `shad->set_uniform(name, ...)` | 重载：1~4 个 int、float、`vec2`~`vec4`、`math::color`（按线性分量写入） |
| `Renderer::Shaders::shaders` / `set_uniform(name, val)` | 实体着色器列表；对列表中每个着色器写入 |

```cpp
using namespace jeecs;

auto shad = graphic::shader::load(nullptr, "!/builtin/shader/Forward2DSelfGlowing.shader");
if (auto* shaders = entity.get_component<Renderer::Shaders>(); shaders && shad.has_value())
{
    shad.value()->set_uniform("SelfGlowing", 2.0f);
    shaders->shaders.push_back(shad.value());
}
```

### 相关术语

参见[术语表](README.md)：

* [纹理](texture.md)——着色器按通道号采样的图像资源；
* [网格](mesh.md)——顶点属性须与着色器的顶点输入结构对应；
* [渲染](render.md)——渲染系统按着色器列表逐 pass 绘制实体；
* [光照](light.md)——2D 光照管线及其专用着色器与 uniform；
* [缓存](cache.md)——着色器缓存 `.je4cache`；[资源镜像](fimg.md)——发布时随包携带着色器缓存。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。HLSL 生成器位于 `je/shader/shader.wo`（基于 `pkg::woshader`），源码到各后端代码的编译链位于 `src/jeecs_graphic_shader_wrapper.cpp`，加载与共享逻辑位于 `src/jeecs_graphic_api_basic.cpp`，数据结构与 C ABI 见 `include/jeecs/graphic.hpp`。

### 加载链路

`je_graphic_shader_load(context, path)`：context 非空时先查共享资源缓存；未命中则 `je_cache_file_load(path, SHADER_CACHE_VERSION, 0)`（以源文件内容计算校验）读取 `.je4cache`，经 `_jegl_load_shader_cache` 还原；缓存不可用时读出源码交给 `je_graphic_shader_source_load(..., is_virtual_file = false)`。Woolang 的 `shader::create` 对应 `is_virtual_file = true`：共享缓存以 vpath 为键，缓存校验码是源码字符串的 CRC64。只有 `m_enable_to_shared`（即 `SHARED(true)`）的实例会登记进共享缓存。

### 着色器包装（shader wrap）的生成

`_jegl_load_shader_source_impl` 在定义了 `JE4_ENABLE_SHADER_WRAP_GENERATOR` 时执行：

1. `wo_load_source` 编译 `.shader` 脚本，在独立的虚拟机中运行顶层代码——渲染状态调用与各 `WOSHADER_*` 声明此时生效；
2. 调用 `je::shader::generate_shader`：woshader 以 `HLSLForGlslang` 生成器产出 HLSL 顶点 / 片元源码（入口 `vertex_main` / `fragment_main`），并收集顶点输入与片元输出布局、采样器、纹理声明（名字、通道、采样器 id、纹理类别）、被用到的 uniform、uniform block 与渲染状态，经原生函数 `jeecs_shader_wrap_result_pack` 打包成 `shader_wrapper::ShaderWrapper`；
3. `jegl_shader_generate_shader_source`：glslang 把 HLSL 编译为 SPIR-V（Vulkan 1.1 / SPIR-V 1.1，预定义 `GLSLANG_HLSL_TO_SPIRV`）；spirv-cross 再把 SPIR-V 转为 GLSL 330、GLSL ES 300 与 MSL 2.0——GLSL 的组合采样器统一命名为 `je4_gl_sampler_<binding>`，顶点输出 / 片元输入统一命名为 `v_out_<location>` 以保证两端匹配；
4. 建立反射表、计算 uniform 布局，最后 `_jegl_create_shader_cache` 写出缓存：五种代码、渲染状态、共享标志、uniform（名字 / 类型 / 标签 / 初值）、uniform block、顶点输入与片元输出布局、采样器与纹理声明。

各后端取用其中一种：DX11 用 HLSL，Vulkan 用 SPIR-V，OpenGL 3.3 用 GLSL 330，OpenGL ES 3.0 / WebGL 2.0 用 GLSL ES 300，Metal 用 MSL 2.0。

⚠️ 注意 着色器包装在**着色器首次加载时**由引擎生成，而不是在构建或 CMake 预构建阶段——`script/prebuild/prebuild.wo` 并不处理着色器。关闭 `JE4_ENABLE_SHADER_WRAP_GENERATOR` 构建的引擎不链接 glslang / spirv-cross，只能读取已有缓存（日志 “Shader generator has been disabled.”）。

### Uniform 布局

生成的 HLSL 把用到的自定义 uniform 放进 `cbuffer SHADER_UNIFORM : register(b0)`（Vulkan 绑定 (0, 0)），按对齐等级（4 / 3 / 2 / 1 分量）降序、同级按名字排序，使 HLSL 的打包结果与 std140 一致。`_jegl_resolve_canonical_uniform_layout` 按 std140 规则（标量 4/4、二维 8/8、三维 12/16、四维 16/16、float2x2 16/8、float3x3 48/16、float4x4 64/16，纹理不占空间）计算每个 uniform 的 `m_offset` / `m_size` 与整块大小 `m_uniform_block_size`，所有后端与命令缓冲录制器共用这一布局。同时按名字解析内建位置表 `m_builtin_uniforms`（`m_builtin_uniform_m` 对应 `JE_M`，依此类推）。

`WOSHADER_UNIFORM_BLOCK!(0)` 声明的 `JE_V` / `JE_P` / `JE_VP` / `JE_TIME` 与 `BasePipelineInterface::default_uniform_buffer_data_t` 布局一致：每个摄像机的 `Camera::Projection::default_uniform_buffer`（绑定位置 0）在相机矩阵更新时写入，并在该摄像机的绘制 pass 开始时绑定。`JE_NDC_SCALE` 是图形库的保留变量，生成的顶点入口把输出位置乘以它，渲染时由图形库自动更新，任何情况下都不要从外部修改。

### 内置 uniform 与 set_uniform 的写入

`je_graphic_pipeline_create` 为 pipeline 缓存 `je_gl_builtin_uniform_slots_t`：`m_model`（`JE_M`）、`m_model_view`（`JE_MV`）、`m_model_view_projection`（`JE_MVP`）、`m_local_scale`、`m_uv_tiling`、`m_uv_offset`、`m_color`、`m_light2d_resolution`、`m_light2d_decay`、`m_shadow_ellipse`、`m_shadow_key`；着色器未声明的项 `m_offset == JE_GL_UNIFORM_SLOT_NONE`。渲染系统的 `draw_shader_passes` 对每个 pass 绑定 pipeline 与顶点，经 `JE_CHECK_NEED_AND_SET_UNIFORM` 只写入已声明的项，再发出索引绘制；pipeline 按（着色器, 图元拓扑）缓存在 `GraphicPipelineBase::m_pipeline_cache` 中。

`graphic::shader::set_uniform` 沿 `m_custom_uniforms` 链表按名字查找，类型匹配时写入 `m_value` 并置 `m_updated` 与 `m_handle.m_modified`。绘制时 `je_graphic_cmd_bind_pipeline` 调用 `reset_uniform_staging`，把着色器全部自定义 uniform 的当前值拷入本次绘制的暂存块（float3x3 每行按 16 字节填充），随后写入内建 uniform——这就是“资源上的值是每次绘制的初始值”的实现，也是共享实例上的修改全局可见的原因。`bound_shader` 的各 setter 则只改写本次绘制的暂存块。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| 着色器 DSL 入口、渲染状态、内置 uniform、HLSL 生成器 | `je/shader/shader.wo`（DSL 本体见 `pkg/woshader`） |
| 2D 光照着色器公共声明 | `je/shader/light2d.wo` |
| HLSL → SPIR-V → GLSL / GLSL ES / MSL、std140 布局、缓存读写 | `src/jeecs_graphic_shader_wrapper.cpp` |
| 加载、源码编译执行、共享缓存 | `src/jeecs_graphic_api_basic.cpp`（`_jegl_load_shader_source_impl`、`je_graphic_shader_load`） |
| 数据结构、pipeline 内建 uniform 槽位与 C ABI | `include/jeecs/graphic.hpp` |
| Woolang 绑定 | `je/graphic.wo`、`src/jeecs_graphic_woolang_api.cpp` |
| 多 pass 绘制、默认着色器 | `src/jeecs_core_graphic_system.cpp`（`draw_shader_passes`、`DefaultResources`） |
| 内置着色器 | `builtin/shader/` |
