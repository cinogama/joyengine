# 射线（Ray）

本文档是[术语表](README.md)中「射线」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述如何从摄像机构造射线、以及引擎提供的相交测试；
* **第二部分·底层实现**——面向引擎维护者，描述屏幕坐标到世界射线的数学推导与各相交算法的细节。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 射线是什么

射线（Ray）由*起点*与*方向*构成：从起点出发、沿方向无限延伸的半直线。射线是纯粹的数学工具而不是组件：C++ 侧为 `jeecs::math::ray`（头文件 `jeecs/ray.hpp`），字段为 `orgin`（源码中即此拼写）与 `direction`，缺省为从原点指向 +Z；Woolang 侧为 `je::towoo::ray`（需要 `import je::towoo`）。

最常见的用途是*拾取*：把屏幕上的点击位置变成一条世界空间中的射线，再找出它最先碰到的实体——编辑器视口的点选实体就是这样实现的。

### 从摄像机构造射线

构造需要四样输入：摄像机实体的 `Transform::Translation` 与 `Camera::Projection`、屏幕位置（**NDC 坐标**：x、y 取值 −1~1，原点在视口中心，y 轴向上），以及“是否为正交投影”的标志。

* **透视**：起点是摄像机的世界位置，方向是指向光标所对远裁剪面上那一点的**单位向量**；
* **正交**：起点是摄像机平面上光标所对的点，方向固定为世界 **+Z**（限制见[第二部分](#从-ndc-到世界射线)）；
* 正交标志必须与摄像机实际的投影组件一致，编辑器的判定是“没有 `Camera::PerspectiveProjection` 即为正交”；
* 构造用到的逆投影矩阵由渲染管线在每帧的图形更新阶段为每个摄像机计算：在逻辑更新中构造射线时，用的是最近一次图形更新的结果；尚未被渲染管线处理过的摄像机，逆投影矩阵仍为全 0，构造出的射线与光标位置无关（透视时方向退化为零向量）。

屏幕像素坐标到 NDC 的换算由调用方完成。基本换算如下（即编辑器在取不到显示空间换算时的回退做法；ImGui 坐标 y 向下，需要翻转）：

```
ndc.x =   (mouse.x - rect_min.x) / rect_width  * 2 - 1
ndc.y = -((mouse.y - rect_min.y) / rect_height * 2 - 1)
```

### 相交测试

| 方法（C++） | 说明 |
| --- | --- |
| `intersect_triangle(v0, v1, v2)` | 与三角形求交（双面） |
| `intersect_rectangle(v0, v1, v2, v3)` | 与四边形求交，拆为 (v0, v1, v2) 与 (v0, v3, v2) 两个三角形 |
| `intersect_box(offset, size, rotation)` | 与以 `offset` 为中心的有向包围盒求交，取 6 个面中最近的命中 |
| `intersect_mesh(mesh, offset, rotation, scale)` | 逐三角形测试网格（仅支持 `TRIANGLES` / `TRIANGLESTRIP`） |
| `intersect_entity(translation, shape, consider_mesh)` | 实体拾取：先测包围盒，`consider_mesh` 为真时再细化到网格 |

* 所有方法返回 `intersect_result`：`intersected`（是否命中）、`place`（命中点）、`distance`（起点到命中点的距离）；**起点背后的交点不算命中**；
* `intersect_entity` 的包围盒：实体有网格时取网格的局部包围盒，乘以实体缩放并按实体旋转放置；带有 `Renderer::Shape` 但未指定网格时按默认四边形取 1 × 1 × 0；`shape` 为空（实体没有 `Renderer::Shape`）时按 1 × 1 × 1 的立方体处理；
* `consider_mesh` 为真且包围盒命中时，返回值改为网格逐三角形测试的结果——可能由命中变为未命中；
* 引擎没有提供与平面、球体等其它形状求交的方法。

Woolang 侧的 `intersect` 即 `intersect_entity`，命中时返回命中点 `option<vec3>`，距离需自行计算。

### API 速览

#### Woolang（`je::towoo::ray`）

| 函数 | 说明 |
| --- | --- |
| `towoo::ray::create(origin, direction)` | 直接给出起点与方向（不做归一化） |
| `towoo::ray::from(trans, proj, ndc, ortho)` | 从摄像机构造 |
| `towoo::ray::intersect(ray, trans, shape, consider_mesh)` | 与实体求交，返回命中点 `option<vec3>` |
| `towoo::ray::origin(ray)` / `towoo::ray::direction(ray)` | 起点 / 方向 |

下例取自编辑器拾取逻辑的简化：在候选实体中找出离起点最近的命中者。

```woolang
import pkg::std;
import je;
import je::ecs;
import je::towoo;
import je::towoo::types;
import je::towoo::components;

using std;
using je;

func pick(camera: entity, ndc: vec2, candidates: entity_selector)=> option<entity>
{
    let trans = camera->towoo::get_component:<Transform::Translation>->unwrap;
    let proj = camera->towoo::get_component:<Camera::Projection>->unwrap;
    let ortho = camera->towoo::get_component:<Camera::PerspectiveProjection>->is_none;
    let ray = towoo::ray::from(trans, proj, ndc, ortho);

    let mut best = option::none: option<entity>;
    let mut best_dist = 0.;
    for (let e : candidates)
    {
        match (e->towoo::get_component:<Transform::Translation>)
        {
            none?;
            value(etrans)?
                match (ray->intersect(etrans, e->towoo::get_component:<Renderer::Shape>, false))
                {
                    none?;
                    value(hit)?
                    {
                        let dist = (hit - ray->origin)->length;
                        if (best->is_none || dist < best_dist)
                        {
                            best = option::value(e);
                            best_dist = dist;
                        }
                    }
                }
        }
    }
    return best;
}
```

#### C++（`jeecs::math::ray`，头文件 `jeecs/ray.hpp`）

构造函数 `ray(origin, direction)` 直接给出起点与方向，`ray(camera_trans, camera_proj, ndc, ortho)` 从摄像机构造；相交方法见上文的表格。

```cpp
using namespace jeecs;

// camera_trans / camera_proj 取自摄像机实体，ndc 为光标的 NDC 坐标
math::ray ray(camera_trans, camera_proj, ndc, /* ortho */ false);

auto hit = ray.intersect_entity(entity_trans, entity_shape_may_null, /* consider_mesh */ false);
if (hit.intersected)
    debug::loginfo("hit (%f, %f, %f), distance %f",
        hit.place.x, hit.place.y, hit.place.z, hit.distance);
```

### 相关术语

参见[术语表](README.md)：

* [摄像机](camera.md)——提供构造射线所需的变换与投影；
* [变换](transform.md)——`Transform::Translation` 的世界位置、旋转与缩放；
* [网格](mesh.md)——包围盒与三角形数据的来源；
* [输入](input.md)——鼠标位置的来源；[数学概念](math.md)——向量、四元数与矩阵；
* [编辑器](editor.md)——视口拾取是射线的典型用例。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。射线是纯头文件实现（`include/jeecs/ray.hpp`）；Woolang 绑定见 `je/towoo/towoo.wo` 与 `src/jeecs_woolang_towoo_script_basic.cpp`；逆投影矩阵由 `include/jeecs/graphic.hpp` 的 `ortho_inv_projection` / `perspective_inv_projection` 生成，在 `src/jeecs_core_graphic_system.cpp` 的 `update_camera_matrices` 中写入 `Camera::Projection::inv_projection`。

### 从 NDC 到世界射线

矩阵以 `float[4][4]` 按列存储（`m[列][行]`），`math::mat4xvec4` 计算“矩阵 × 列向量”。构造函数取 `p = inv_projection × (x, y, 1, 1)`，w 不为 0 时把 xyz 除以 w：

* **透视**（设 `t = tan(fov / 2)`、`a` 为宽高比）：`p = (a·t·x, t·y, 1, 1/zfar)`，除以 w 后为 `(a·t·x·zfar, t·y·zfar, zfar)`，即视图空间中光标对应的远裁剪面上的点（视图空间朝 +Z 看）。`origin = world_position`，`direction = (world_rotation × p).unit()`；
* **正交**（`R = a·5 / scale`、`T = 5 / scale`，5 即 `ORTHO_PROJECTION_RATIO`）：`p = (R·x, T·y, zfar − znear, 1)`，w 恒为 1。`origin = world_position + world_rotation × (R·x, T·y, 0)`，`direction = (0, 0, 1)`。

两条分支得到的方向都是单位向量；`ray(origin, direction)` 构造函数与 Woolang 的 `create` 则原样保存传入的方向，不做归一化——`distance` 始终是命中点到起点的欧氏距离，与方向长度无关。宽高比取自摄像机的实际渲染目标（`RendToFramebuffer` 的帧缓冲或窗口，再乘以 `Camera::Viewport` 的宽高比例），`update_camera_matrices` 在各渲染管线系统 `GraphicUpdate` 开头的 `begin_frame` 中执行。

⚠️ 注意 正交分支的方向没有乘以摄像机旋转，只对朝向世界 +Z 的摄像机（典型的 2D 摄像机）正确，而起点的平面偏移却随旋转变化。分支完全由 `ortho` 参数决定——源码注释中“w 不为 0 即透视”的判断对正交矩阵同样成立，并不能区分投影类型，参数与摄像机实际投影不符时得到的射线没有意义。

### 相交算法

* `intersect_triangle` 采用 Möller–Trumbore：`E1 = v1 − v0`、`E2 = v2 − v0`、`P = d × E2`、`det = E1 · P`，按 det 的符号统一朝向（因此双面），`det < 0.0001` 视为平行；重心坐标 u、v 越界即未命中；命中点用 `(1 − u − v)·v0 + u·v1 + v·v2` 重算，`(命中点 − 起点) · d < 0` 时视为在起点背后。det 阈值是未归一化的绝对值，随方向长度与三角形边长缩放，非常小的三角形可能被误判为不相交；
* `intersect_box` 由 `±size / 2` 生成 8 个角点，经 `rotation` 旋转再加上 `offset`，对 6 个面调用 `intersect_rectangle` 取最近者；
* `intersect_mesh` 取每个顶点的前 3 个 float 作为位置（假定第一个属性是位置），变换为 `offset + (rotation × p) × scale`——缩放在旋转之后按世界轴分量相乘，只有等比缩放时才等价于常规的“先缩放后旋转”；
* `intersect_entity`：包围盒中心 = 网格包围盒中心 + `world_position`（中心偏移不随旋转和缩放）、尺寸 = `local_scale × 包围盒尺寸`、旋转 = `world_rotation`；`consider_mesh` 时以 `(world_position, world_rotation, local_scale)` 调用 `intersect_mesh` 并直接返回其结果。

⚠️ 注意 `intersect_mesh` 没有查询索引缓冲：`TRIANGLES` 分支在第 i 个索引位置直接读取第 i 个顶点，`TRIANGLESTRIP` 分支按顶点存储顺序滑动，而循环上界是 `m_index_count`。只有索引恰为 0、1、2… 且索引数不超过顶点数时结果才正确（如默认四边形）；对模型文件加载的网格，网格级测试并不可靠，还可能越界读取顶点数据。

### Woolang 绑定

`towoo::ray` 是包装 `new math::ray(...)` 的 gchandle：`from` 直接读取 towoo 组件实例（`Transform::Translation`、`Camera::Projection`）与 `vec2` / `bool` 参数调用摄像机构造函数；`intersect` 把 `option<Renderer::Shape>` 转为可空指针调用 `intersect_entity`，只返回命中点 `place`；`origin` / `direction` 读出两个字段。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| `math::ray` 与全部相交算法 | `include/jeecs/ray.hpp` |
| 正交 / 透视投影及其逆矩阵 | `include/jeecs/graphic.hpp`（`ortho_inv_projection`、`perspective_inv_projection`） |
| 每帧写入 `inv_projection` | `src/jeecs_core_graphic_system.cpp`（`update_camera_matrices`） |
| `mat4xvec4` 与四元数旋转 | `include/jeecs/math.hpp` |
| Woolang 绑定 | `je/towoo/towoo.wo`、`src/jeecs_woolang_towoo_script_basic.cpp` |
| 编辑器拾取实践参考 | `builtin/editor/Editor/WorldEditorViewport/EntityPicking.wo`、`CameraControl.wo` |
