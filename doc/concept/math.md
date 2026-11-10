# 数学概念

本文档是[术语表](README.md)中「数学概念」小节的详细说明，覆盖其中的四个词条——**标量和矢量**、**矩阵**、**四元数**、**颜色**，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述各数学类型的含义、约定与 C++ / Woolang 两侧的用法；
* **第二部分·底层实现**——面向引擎维护者，描述数学库的实现方式、内存布局约定以及两种语言之间的类型桥接。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 两套接口，同一组类型

C++ 的 `jeecs::math`（头文件 `jeecs/math.hpp`）与 Woolang 脚本层 towoo（`import je::towoo; import je::towoo::types;`）提供同一组数学类型，组件成员、着色器参数在两侧之间传递时由引擎自动转换：

| 类型 | 分量 | C++ | Woolang | 典型用途 |
| --- | --- | --- | --- | --- |
| `ivec2` | 2 个整数 | `math::ivec2` | `(int, int)` | 整数坐标、尺寸 |
| `vec2` | 2 个浮点数 | `math::vec2` | `(real, real)` | 2D 位置、UV、屏幕坐标 |
| `vec3` | 3 个浮点数 | `math::vec3` | `(real, real, real)` | 位置、缩放、方向 |
| `vec4` | 4 个浮点数 | `math::vec4` | `(real, real, real, real)` | 齐次坐标、视口、着色器参数 |
| `quat` | x、y、z、w | `math::quat` | `(real, real, real, real)` | 旋转 |
| `color` | r、g、b、a | `math::color` | `(real, real, real, real)` | 颜色 |
| `mat4` | 4×4 浮点数 | `float[4][4]` | 16 个 `real` 的元组 | 变换与投影 |

Woolang 侧这些类型都是元组，用 `(1., 2., 3.) as vec3` 构造，用 `v[0]` 取分量。另有 `pkg::math` 包提供通用的标量数学函数（三角函数、指数、取整等），它不包含矢量类型。

### 标量和矢量

*标量*只有大小，*矢量*既有大小也有方向。标量直接使用 `float`/`int`（Woolang 为 `real`/`int`）；矢量使用 `ivec2`、`vec2`、`vec3`、`vec4`。

| 运算 | C++ | Woolang |
| --- | --- | --- |
| 加、减 | `a + b`、`a - b`、`-a` | `a + b`、`a - b` |
| 逐分量乘、除 | `a * b`、`a / b` | `a * b`、`a / b` |
| 数乘、数除 | `a * k`、`k * a`、`a / k` | `a * k`、`a / k`（**标量只能写在右侧**） |
| 长度、单位化 | `a.length()`、`a.unit()` | `a->length`、`a->unit` |
| 点积 | `a.dot(b)` | `a->dot(b)` |
| 叉积（仅 `vec3`） | `a.cross(b)` | `a->cross(b)` |
| 其他 | `vec2::rotate(度)`、`min()`/`max()`、`==`/`!=` | — |

`ivec2` 只有上表中的四则运算，没有长度、点积等浮点运算；零向量的 `unit` 返回零向量本身。

```cpp
#include "jeecs/math.hpp"
using namespace jeecs;

math::vec3 a(1.f, 0.f, 0.f), b(0.f, 1.f, 0.f);
math::vec3 c = a + 2.f * b;                         // (1, 2, 0)
float d = a.dot(b);                                 // 0
math::vec3 n = a.cross(b).unit();                   // (0, 0, 1)
math::vec2 r = math::vec2(1.f, 0.f).rotate(90.f);   // 绕原点旋转 90 度，约为 (0, 1)
```

```woolang
import je::towoo;
import je::towoo::types;

let a = (1., 0., 0.) as vec3;
let b = (0., 1., 0.) as vec3;

let c = a + b * 2.;          // (1, 2, 0)
let d = a->dot(b);           // 0
let n = a->cross(b)->unit;   // (0, 0, 1)
```

⚠️ 注意 Woolang 侧的 `ivec2` 目前只能做加减：其乘除运算符的实现把结果标注为 `: ivec2` 而不是转换 `as ivec2`，实例化时编译失败。需要缩放整数坐标时请逐分量计算。

### 矩阵

*矩阵*（Matrix）描述线性变换。JoyEngine 统一使用 4×4 矩阵表示变换与投影，并约定**矢量是列向量、写在矩阵右侧**：`M × v`；组合变换按从右到左的顺序生效，例如 `MVP = P × V × M`。

C++ 侧**没有矩阵类**：矩阵就是 `float[4][4]` 数组，运算由自由函数完成——

| 函数 | 作用 |
| --- | --- |
| `math::mat4xmat4(out, left, right)` | `out = left × right` |
| `math::mat4xvec4(out, m, v)` | `out = m × v` |
| `math::mat4trans(m, v)` | 变换 `vec3`（自动补 w = 1 并做透视除法）或 `vec4`（不做除法），返回新矢量 |
| `math::transform(out, pos, rot, scale)` | 构造 `T × R × S` 变换矩阵 |

Woolang 侧的 `mat4` 支持 `mat4 * mat4`、`mat4 * vec4` 与 `mat4::transform(pos, rot, scale)`，四元数可以用 `quat::to_mat` 转为矩阵。

**常用矩阵不需要手写，直接从组件读取**：

* 物体到世界矩阵：`Transform::Translation` 组件的 `object2world`，由引擎每帧根据实体的世界位置、旋转与缩放计算；
* 摄像机矩阵：`Camera::Projection` 组件的 `view`、`projection`、`view_projection`、`inv_projection`，由渲染流程每帧根据摄像机的位置、朝向与投影参数计算（Woolang：`get_view_mat`、`get_proj_mat`、`get_view_proj_mat`、`get_inv_proj_mat`）。

这些矩阵每帧都会被重新计算，改写它们没有意义；要移动物体或调整摄像机，应修改 `LocalPosition`、`LocalRotation`、`LocalScale` 或投影组件的参数。

```cpp
// projection 为摄像机实体上的 Camera::Projection 组件
float model[4][4];
math::transform(model, math::vec3(0.f, 1.f, 0.f), math::quat(), math::vec3(1.f, 1.f, 1.f));

float mvp[4][4];
math::mat4xmat4(mvp, projection.view_projection, model);           // MVP = VP × M
math::vec3 ndc = math::mat4trans(mvp, math::vec3(0.f, 0.f, 0.f));  // 变换一个点
```

```woolang
// proj 为 Camera::Projection，pos / rot / scale 为实体的位置、旋转与缩放
let model = mat4::transform(pos, rot, scale);         // T × R × S
let mvp = proj->get_view_proj_mat * model;            // MVP = VP × M
let clip = mvp * ((0., 0., 0., 1.) as vec4);          // 变换一个点（手动补 w = 1）
```

### 四元数

*四元数*（Quaternion）是表示三维旋转的常用形式，由 x、y、z、w 四个分量组成，表示旋转的四元数应为单位长度。与欧拉角相比，它没有万向节死锁问题，在两个旋转之间插值也更平滑。`Transform::LocalRotation` 等组件都用 `quat` 保存旋转；编辑器的检视器则把它显示为以**度**为单位的三个欧拉角，便于编辑。欧拉角的三个参数依次是绕 **X、Y、Z** 轴的角度。

| 操作 | C++（`math::quat`） | Woolang（`quat`） |
| --- | --- | --- |
| 不旋转（单位四元数） | `math::quat()` | `quat::ident` |
| 由欧拉角构造（度） | `quat::euler(x, y, z)` | `quat::euler(x, y, z)` |
| 由轴角构造（度，轴须为单位向量） | `quat::axis_angle(axis, deg)` | `quat::axis(axis, deg)` |
| 组合旋转（先 `q2` 后 `q1`） | `q1 * q2` | `q1 * q2` |
| 旋转一个矢量 | `q * v` | `q * v` |
| 逆旋转 | `q.inverse()` | `q->inverse` |
| 转回欧拉角（度） | `q.euler_angle()` | `q->euler_angle` |
| 插值 | `quat::lerp`、`quat::slerp` | `quat::lerp`、`quat::slerp` |
| 两个旋转的夹角（度） | `quat::delta_angle(a, b)` | `quat::delta_angle(a, b)` |
| 转为矩阵 | `q.create_matrix(m)` | `q->to_mat` |

```cpp
math::quat q = math::quat::euler(0.f, 90.f, 0.f);                       // 绕 Y 轴 90 度
math::vec3 dir = q * math::vec3(0.f, 0.f, 1.f);                         // (1, 0, 0)
math::quat q2 = q * math::quat::axis_angle(math::vec3(1.f, 0.f, 0.f), 30.f);
math::quat mid = math::quat::slerp(q, q2, 0.5f);                        // 球面插值
```

```woolang
let q = quat::euler(0., 90., 0.);                     // 绕 Y 轴 90 度
let dir = q * ((0., 0., 1.) as vec3);                 // (1, 0, 0)
let q2 = q * quat::axis((1., 0., 0.) as vec3, 30.);
let mid = quat::slerp(q, q2, 0.5);
let angles = mid->euler_angle;                        // 欧拉角（度）
```

### 颜色

*颜色*（color）以 r、g、b、a 四个浮点分量表示，通常取值 0~1。`math::color` 默认构造为不透明白色 `(1, 1, 1, 1)`，构造时省略 a 则取 1。使用颜色的组件包括 `Renderer::Color`（实体着色，默认白色）、`Camera::Clear`（清屏颜色，默认黑色），以及粒子发射器、2D 光照等的颜色参数。

**颜色按线性颜色空间保存**：组件里存的是线性值，交给着色器时直接使用；RGB 允许大于 1 表示 HDR 颜色，alpha 不参与颜色空间转换。而人眼习惯的“取色器数值”是 sRGB 编码的，两者之间需要转换：

| 转换 | C++ | Woolang |
| --- | --- | --- |
| sRGB → 颜色 | `math::color::from_srgb(vec4)` | `color::from_srgb(v)` |
| 颜色 → sRGB（显示用） | `c.to_srgb()` | `c->to_srgb` |
| 线性 `vec4` ↔ 颜色 | `color::from_linear(v)`、`c.to_linear()` | `color::from_linear(v)`、`c->to_linear` |

编辑器的颜色选择器显示与拾取的是 sRGB 值，写回组件时再转换为线性值，所以检视器里看到的颜色与组件中存储的数字并不相同（例如 sRGB 的 0.5 约等于线性的 0.214）。从 0~255 的 8 位 sRGB 数值换算时，先除以 255，再经 `from_srgb` 转为颜色。

```cpp
math::color white;                                   // (1, 1, 1, 1)
math::color orange = math::color::from_srgb(         // 由 sRGB 的 0~255 数值换算
    math::vec4(255.f / 255.f, 128.f / 255.f, 0.f, 1.f));
math::vec4 to_shader = orange.to_linear();           // 交给着色器的线性值
```

```woolang
let picked = (255. / 255., 128. / 255., 0., 1.) as vec4;   // sRGB 数值
let c = color::from_srgb(picked);                         // 线性颜色
let shown = c->to_srgb;                                   // 显示用的 sRGB 值
```

### 角度与弧度

引擎面向使用者的角度**一律使用度**：四元数的欧拉角与轴角、`vec2::rotate`、透视投影的视角 `Camera::PerspectiveProjection::angle`、UI 的 `Rotation::angle`、粒子的散布角等。三角函数（C++ 的 `std::sin`，Woolang 的 `je::mathf::sin`、`pkg::math` 的 `math::sin`）接收**弧度**，换算使用 `DEG2RAD` / `RAD2DEG` 常量（C++：`math::DEG2RAD`；Woolang：`je::mathf::DEG2RAD`，如 `je::mathf::sin(45. * je::mathf::DEG2RAD)`）。

### 相关术语

参见[术语表](README.md)：

* [变换](transform.md)——`LocalPosition`/`LocalRotation`/`LocalScale` 与世界变换、物体到世界矩阵；
* [摄像机](camera.md)——投影组件与摄像机矩阵；
* [射线](ray.md)——由起点与方向（`vec3`）构成；
* [着色器](shader.md)——`vec`/`mat4` 作为 uniform 参数；
* [动画](animation.md)——旋转轨道在源文件中以欧拉角记录，缓存中保存为四元数。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。C++ 数学库全部位于头文件 `include/jeecs/math.hpp`；Woolang 侧的类型声明由引擎生成到 `je/towoo/types.wo`，运算实现在 `je/towoo/towoo.wo`，其中调用 C++ 的部分绑定于 `src/jeecs_woolang_towoo_script_basic.cpp`；类型桥接机制见 `include/jeecs/typing.hpp`。

### 实现风格

`jeecs::math` 是纯头文件实现：矢量是只含 float 成员的结构体（`vec2`/`vec3`/`vec4` 继承自聚合体 `_basevec2/3/4`），运算符全部是 `inline constexpr` 成员函数；`quat`、`color` 同样是普通结构体；矩阵运算是对 `float*` 与 `float(*)[4]` 两种形式重载的自由函数；另有 `almost_equal`、`clamp`、`lerp`、`random` 等通用模板函数。实现中**没有 SIMD**，也没有对齐要求——内存布局就是连续的 float，组件存储与脚本转换都依赖这一点。

⚠️ 注意 矢量类型之间存在**隐式转换**：高维转低维（如 `vec4` → `vec3`、`vec3` → `vec2`）静默丢弃多余分量，低维转高维时缺失分量补 0——`vec4(v3)` 的 w 为 0，表示的是方向而非点，用 `mat4trans(m, vec4)` 变换点时须显式写出 w = 1。

⚠️ 注意 `math::random` 使用函数内静态的 `std::mt19937`，没有任何同步；多个世界或系统并行调用同一实例化版本（例如都以 `float` 调用）时存在数据竞争。

### 矩阵的内存布局与乘法约定

`float m[4][4]` 的第一维是**列**、第二维是**行**（数学意义上的列主序）：元素 `m[c][r]` 是第 r 行第 c 列，平移量位于 `m[3][0..2]`，展平后的线性下标为 `c * 4 + r`。`mat4xvec4` 计算 `out[r] = Σ m[k][r] · v[k]`，即矢量作为列向量右乘；`mat4xmat4(out, L, R)` 计算 `L × R`。

引擎内的组合顺序都遵循这一约定：`transform` 生成 `T × R × S`；变换系统在提交阶段用它从世界位置、旋转、缩放计算 `object2world`；渲染流程计算 `view = R⁻¹ × T⁻¹`、`view_projection = P × V`，绘制时 `MV = V × M`、`MVP = VP × M`。投影矩阵由 `jeecs/graphic.hpp` 中的 `graphic::ortho_projection` / `perspective_projection` 生成，后者的视角以度为单位传入。矩阵原样以 16 个连续 float 写入 uniform 缓冲。

Woolang 的 `mat4` 是 16 个 `real` 的元组，第 i 个元素与 C++ 展平后的第 i 个 float 一一对应（`wo_mat4` / `wo_set_mat4`）。`mat4 * vec4`、`mat4 * mat4`、`mat4::transform`、`quat::to_mat` 都经 extern 调用 C++ 的 `mat4xvec4`、`mat4xmat4`、`transform`、`create_matrix`。

### 四元数的约定

* 四分量构造函数 `quat(x, y, z, w)` 会**自动单位化**（全零输入得到 NaN），`set(x, y, z, w)` 则原样赋值；默认构造为单位四元数 `(0, 0, 0, 1)`；
* `inverse()` 与 `conjugate()` 都返回 `(-x, -y, -z, w)`，前提是四元数为单位长度；
* `q * v` 使用 `2(u·v)u + (w² − u·u)v + 2w(u × v)` 旋转矢量；`q1 * q2` 为 Hamilton 积，作用于矢量时先 `q2` 后 `q1`——`Translation::get_parent_rotation` 中的 `world_rotation * local.inverse()` 即据此反推父级旋转；
* `slerp` 在两者点积为负时取反走短弧，几乎重合时退化为线性插值；
* 欧拉角（`set_euler_angle`、`euler`、三参数构造函数）的合成顺序为 `q = q_Z · q_Y · q_X`，即先绕 X、再绕 Y、最后绕 Z（均为固定的世界轴）；`euler_angle()` 是对应的逆运算，Y 分量经 `asin` 计算，范围限于 ±90 度。

⚠️ 注意 欧拉角参数在源码中命名为 `yaw, pitch, roll`，但它们依次对应绕 **X、Y、Z** 轴的旋转，与“yaw 绕竖直轴”的常见习惯不同。另外 C++ 的 `math::quat(a, b, c)` 是欧拉角（度），`math::quat(a, b, c, d)` 是四个分量，两者只差一个参数，容易混淆。

⚠️ 注意 `quat::rotation(a, b)` 以 `b × a` 为旋转轴，得到的是**把 b 转到 a** 的旋转（`rotation(a, b) * b` 与 a 同向），与名字的直觉相反。

⚠️ 注意 两侧的插值并不完全一致：C++ 的 `quat::lerp` 与 `axis_angle` 经四分量构造函数自动单位化；Woolang 的 `quat::lerp` 与 `quat::axis` 直接返回元组，不做单位化。Woolang 的 `slerp` 调用 C++ 实现，两侧结果一致。

### 颜色空间的实现

`math::color` 的 `to_linear()` 只是把分量原样打包成 `vec4`；`to_srgb()` / `from_srgb()` 逐通道调用 `linear_to_srgb` / `srgb_to_linear`，即标准 sRGB 传输函数（分段阈值 0.04045 / 0.0031308、线性段系数 12.92、指数 2.4），负输入按 0 处理，大于 1 的 HDR 分量在转换后仍大于 1。渲染系统、UI、粒子与 2D 光照把颜色交给图形管线（uniform、清屏颜色）时都统一使用 `to_linear()` 的结果。

编辑器的检视器扩展（项目中的 `resource/editor/editor.wo`，模板为 `builtin/script/editor.wo`）实现了上文的显示约定：颜色先 `to_srgb` 再交给 `ColorEdit4`，写回时 `from_srgb`；四元数先转为 `euler_angle` 再用 `InputFloat3` 编辑，写回时经 `quat::euler`。序列化扩展（`resource/editor/types.wo`，模板为 `builtin/script/types.wo`）把矢量、四元数与颜色保存为数组文本（如 `[x,y,z]`），颜色存的是线性值，缺失的 alpha 按 1 恢复。

纹理一侧的衔接：图片纹理在颜色空间为 sRGB（默认值）时带 `JE_GL_TEXTURE_FORMAT_SRGB` 标志，由硬件在采样时把 sRGB 像素解码为线性值，像素存储本身不变；CPU 侧的像素访问器 `graphic::texture2d::pixel` 以 `vec4` 读写 0~1 的值，与 8 位数据之间按 `/255` 和“钳制到 [0, 1] 后 ×255 取整”换算，这里读写的是原始存储值，不做颜色空间转换。

### C++ 与 Woolang 的类型桥接

每个数学类型都遵循同一组约定接口：静态函数 `JEScriptTypeName()`（如 `"vec3"`）与 `JEScriptTypeDeclare()`（如 `"public using vec3 = (real, real, real);"`），以及逐分量读写脚本结构体的成员函数 `JEParseFromScriptType` / `JEParseToScriptType`。`typing::register_type` 通过概念 `has_JEScriptTypeInterface` 检测到它们后，调用 `je_typing_script_parser_register` 登记转换函数与 Woolang 声明。

`je_towoo_api_update`（脚本侧 `je::towoo::update_api`）遍历所有已登记的类型，把这些声明写成自动生成的 `je/towoo/types.wo`，并把组件成员生成为 `je/towoo/components.wo` 中的 `je::towoo::member<vec3, vec3::type>` 等字段。因此 Woolang 中的 `vec3` 等就是 C++ 声明出来的元组类型；基础运算以纯 Woolang 实现在 `towoo.wo` 中，`slerp`、矩阵运算与 sRGB 转换则经 extern 调用 C++，保证两侧数值一致。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| C++ 数学库（矢量、四元数、颜色、矩阵函数） | `include/jeecs/math.hpp` |
| 投影矩阵辅助函数 | `include/jeecs/graphic.hpp`（`graphic::ortho_projection`、`perspective_projection` 等） |
| 物体到世界矩阵的计算 | `src/jeecs_core_translation_system.cpp`（`CommitUpdate`） |
| 摄像机矩阵与 MVP 的计算 | `src/jeecs_core_graphic_system.cpp` |
| Woolang 类型声明（自动生成） | `je/towoo/types.wo`、`je/towoo/components.wo` |
| Woolang 运算与 `je::mathf` | `je/towoo/towoo.wo` |
| Woolang 数学绑定与生成器 | `src/jeecs_woolang_towoo_script_basic.cpp` |
| 脚本类型桥接 | `include/jeecs/typing.hpp`（`has_JEScriptTypeInterface`、`register_script_parser`） |
| 检视器与序列化模板 | `builtin/script/editor.wo`、`builtin/script/types.wo` |
| 标量数学包 | `pkg/math/math.wo`（`pkg::math`） |
