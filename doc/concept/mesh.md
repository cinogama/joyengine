# 网格（Mesh）

本文档是[术语表](README.md)中「网格」一词的详细说明，内容分为两个层级：

* **第一部分·功能说明**——面向使用引擎开发游戏的开发者，描述网格的来源、顶点格式、如何挂到实体上以及包围盒的作用；
* **第二部分·底层实现**——面向引擎维护者，描述顶点资源的结构、模型文件的解析与各后端顶点缓冲的创建。

开发者只需阅读第一部分；想深入理解行为背后的机制时，再下潜到第二部分。

---

## 第一部分 · 功能说明

### 网格是什么

网格（Mesh）是参与渲染的几何数据：一组*顶点*（每个顶点带有位置、UV 等若干属性）加上一组*索引*（说明顶点如何连成图元）。引擎中的网格资源称为 **vertex**（Woolang `je::graphic::vertex`，C++ `jeecs::graphic::vertex`），和[纹理](texture.md)、[着色器](shader.md)一样是**引用计数的共享资源**。

网格只描述形状本身：它画在哪里由实体的[变换](transform.md)决定，以什么方式着色由着色器决定。

### 图元类型

| 类型（Woolang `vertex::type`） | 含义 |
| --- | --- |
| `LINESTRIP` | 折线：相邻索引两两相连 |
| `TRIANGLES` | 三角形列表：每 3 个索引构成一个三角形 |
| `TRIANGLESTRIP` | 三角形带：从第 3 个索引起，每个索引与前两个构成一个三角形 |

### 网格从哪里来

#### 从模型文件加载

`vertex::load(universe, path)` 从模型文件加载，失败时返回 `none`。模型由 Assimp 解析，编辑器按 `.obj` / `.fbx` / `.gltf` / `.glb` / `.dae` 等常见扩展名识别模型文件。加载结果：

* 始终是 `TRIANGLES` 图元（多边形面被三角化，缺失的法线自动生成）；
* 文件中的所有子网格**合并为一个**网格；
* 顶点格式固定为：位置 3 + UV 2 + 法线 3 + 切线 3 + 骨骼索引 4（整数）+ 骨骼权重 4，共 76 字节。切线由引擎根据 UV 计算，每个顶点最多保留权重最大的 4 个骨骼。

与纹理相同，传入宇宙时**同一路径只加载一次、得到同一份资源**；传入 `option::none` 则每次重新加载。

#### 程序化创建

`vertex::create(type, vertices, indices, formats)`：

* `vertices`：所有顶点的属性**交错排列**成的一维实数数组；
* `indices`：索引数组；
* `formats`：每个顶点属性的分量个数，例如 `[3, 2]` 表示“位置(3) + UV(2)”，不能为空。Woolang 创建的属性一律是 32 位浮点；
* **第一个属性应当是 3 分量的位置**——引擎据此计算包围盒，否则包围盒为全 0 并记录警告；
* 数据中连一个完整顶点都不含时返回 `none`。

程序化创建的网格没有资源路径，`path()` 返回 `none`；场景保存只记录有路径的网格。

#### 内容不可变与动态网格

顶点数据在创建时确定，创建后不能从外部改写（Woolang 侧没有更新接口）。逐帧变化的动态网格（粒子、UI 文本）由引擎在 C++ 侧以命令的形式录制更新（`je_graphic_cmd_update_vertex`），随命令缓冲区提交生效；偶发变化的网格（瓦片地图分片）在内容变化时直接**重建整个网格资源**替换引用。

#### 引擎生成的网格

引擎自身也会程序化生成网格：`Renderer::Shape` 未指定网格时使用的默认四边形（见下文）；粒子系统为每个发射器维护一个按容量创建的动态网格，经命令更新逐帧改写内容并写入其 `Renderer::Shape`；UI 文本同样按容量创建、逐帧经命令更新；瓦片地图分片在内容变化时重建资源。

### 顶点属性与着色器

网格的属性按顺序对应着色器 `WOSHADER_VERTEX_IN!` 结构的字段：第 0 个属性对应第一个字段，依此类推。网格可以提供比着色器所需更多的属性（如内置四边形还带有法线和切线），但前面的属性顺序必须与着色器的要求一致。

### 挂到实体：Renderer::Shape

实体通过 `Renderer::Shape` 组件引用网格（`entity::set_shape(e, option::value(v))`，传 `option::none` 移除；`entity::get_shape(e)` 读取）。**接口只在实体已挂有 `Renderer::Shape` 时生效**，否则静默无效果。

* `Renderer::Shape` 中没有网格时，渲染系统使用**默认四边形**：位于 XY 平面、中心在原点、边长 1 的 `TRIANGLESTRIP`，带 UV、法线与切线；
* 常规渲染管线要求实体同时带有 `Transform::Translation`、`Renderer::Shape` 与 `Renderer::Shaders`（见[渲染](render.md)）。

### 包围盒（AABB）

创建网格时，引擎根据所有顶点的位置计算局部空间的轴对齐包围盒，`vertex::aabb(v)` 返回 `(min_x, min_y, min_z, max_x, max_y, max_z)`。包围盒用于：

* **视锥剔除**：摄像机挂有 `Camera::FrustumCulling` 时，渲染系统用包围盒（乘以实体缩放）估算实体的包围球，完全在视锥外的实体不绘制；
* **射线拾取**：[射线](ray.md)先与包围盒（按实体变换放置）求交，命中后才可选地逐三角形精确测试；
* 2D 光照计算实体的“脚底”位置等。

### API 速览

#### Woolang（`je::graphic::vertex`，实体接口在 `je::ecs`）

| 函数 | 说明 |
| --- | --- |
| `vertex::load(universe, path)` | 从模型文件加载，失败返回 `none` |
| `vertex::create(type, vertices, indices, formats)` | 程序化创建，失败返回 `none` |
| `vertex::aabb(v)` / `vertex::path(v)` | 局部包围盒；资源路径（可能为 `none`） |
| `entity::set_shape(e, v)` / `entity::get_shape(e)` | 实体的网格 |

```woolang
import pkg::std;
import je;
import je::ecs;
import je::graphic;

using je;
using je::graphic;

func setup_mesh(e: entity, univ: universe)
{
    // 每个顶点 = 位置(3) + UV(2)
    let quad = vertex::create(
        vertex::type::TRIANGLESTRIP,
        [
            -0.5,  0.5, 0.,  0., 1.,
            -0.5, -0.5, 0.,  0., 0.,
             0.5,  0.5, 0.,  1., 1.,
             0.5, -0.5, 0.,  1., 0.,
        ],
        [0, 1, 2, 3],
        [3, 2]);

    match (vertex::load(option::value(univ), "@/model/hero.fbx"))
    {
        value(model)? e->set_shape(option::value(model)); // 实体须已挂 Renderer::Shape
        none? e->set_shape(quad);
    }
}
```

#### C++（`jeecs::graphic::vertex`，头文件 `jeecs/graphic.hpp`）

| 方法 | 说明 |
| --- | --- |
| `vertex::load(context, path)` | 返回 `std::optional`；context 为 nullptr 时不参与共享 |
| `vertex::create(type, data, size, indices, formats)` | 属性可为 `JE_GL_VERTEX_DATA_TYPE_FLOAT32` 或 `INT32` |
| `v->update_buffer(data, size, active_index_count)` | 动态更新 |
| `Renderer::Shape::vertex` | `basic::optional<basic::resource<vertex>>`，为空即默认四边形 |

```cpp
using namespace jeecs;

const float quad[] = { -0.5f, 0.5f, 0.f,  -0.5f, -0.5f, 0.f,  0.5f, 0.5f, 0.f,  0.5f, -0.5f, 0.f };
auto mesh = graphic::vertex::create(
    JE_GL_VERTEX_TYPE_TRIANGLESTRIP, quad, sizeof(quad),
    { 0, 1, 2, 3 },
    { { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3 } });

if (auto* shape = entity.get_component<Renderer::Shape>(); shape && mesh.has_value())
    shape->vertex = mesh.value();
```

### 相关术语

参见[术语表](README.md)：

* [着色器](shader.md)——顶点输入结构须与网格属性顺序对应；
* [纹理](texture.md)——按网格提供的 UV 采样；
* [变换](transform.md)——决定网格在世界中的位置、旋转与缩放；
* [渲染](render.md)——渲染系统绘制网格并做视锥剔除；
* [射线](ray.md)——基于包围盒与三角形的拾取；
* [粒子](particle.md)、[瓦片地图](tilemap.md)——在 C++ 侧程序化构建网格的系统。

---

## 第二部分 · 底层实现

> 本部分面向引擎维护者。`je_gl_vertex_t` 与 C ABI 定义在 `include/jeecs/graphic.hpp`，创建、模型加载与动态更新实现在 `src/jeecs_graphic_api_basic.cpp`，Woolang 绑定见 `je/graphic.wo` 与 `src/jeecs_graphic_woolang_api.cpp`。

### je_gl_vertex_t 的结构

`je_gl_vertex_t` 以资源句柄 `m_handle` 开头（偏移 0），之后依次是局部包围盒 `m_x/y/z_min/max`、顶点数据 `m_vertexs` / `m_vertex_length`（字节）、索引 `m_indices`（uint32）/ `m_index_count`（参与绘制的数量，逻辑线程侧的绘制元数据）/ `m_index_capacity`（创建时的数量）、属性布局 `m_formats`（`je_gl_vertex_data_layout_t`：`m_type` 为 INT32 或 FLOAT32，`m_count` 为分量数）、每顶点字节数 `m_data_size_per_point`（两种类型都按 4 字节计）、图元类型 `m_type`，以及骨骼表 `m_bones` / `m_bone_count`（`je_gl_vertex_bone_data_t`：名字、索引、模型到骨骼的变换矩阵）。

`_jegl_create_vertex_impl` 拷贝顶点、索引与布局，在第一个属性是 FLOAT32 × 3 时遍历**全部顶点**（而非索引引用到的顶点）计算包围盒；数据长度不是每顶点字节数的整数倍时只记录警告。Woolang 的 `vertex::create` 把 `formats` 中每个整数转为 `{FLOAT32, n}`。`m_vertexs` 在创建后归图形线程所有：只有命令缓冲区提交时（`je_graphic_cmd_update_vertex` 录制的更新）才会改写它，一旦 `m_dynamic` 被置位，逻辑线程（如射线求交）不再读取其内容。

### 模型文件的解析

`je_graphic_vertex_load` 先查共享资源缓存，未命中时用 `Assimp::Importer`（IO 经 `je_file_io_system` 走引擎文件系统，因此支持 `@/`、`!/` 路径与资源镜像）以 `aiProcess_Triangulate | aiProcess_GenNormals` 读取场景，然后用栈遍历节点树（每节点携带从根累计下来的变换矩阵）：

* 顶点先经节点累计变换烘焙到世界（节点局部）坐标后再追加到同一数组，索引加上该子网格的顶点偏移；法线取变换的线性部分后归一化；
* 读取第 0 套 UV 与法线；切线按每个三角形的边与 UV 差计算，累加到三个顶点上，最后归一化（不使用模型自带的切线）；
* 骨骼权重写入每顶点 4 个槽位：优先占用空槽，槽满后新权重大于其中最小者时替换它；骨骼索引不小于 256 的权重被跳过；未用的槽位最终置为索引 0、权重 0；
* 结果以 `TRIANGLES` 与 76 字节的标准格式创建（位置 3 + UV 2 + 法线 3 + 切线 3 + 骨骼 4 + 权重 4，见 `jegl_standard_model_vertex_format`），带上骨骼表与资源路径，登记进共享缓存。

### 命令化的内容更新

`je_graphic_cmd_update_vertex(cmd, vertex, offset, data, size)` 在**录制时**把数据快照进命令缓冲区，在**提交时**（图形线程）按录制顺序写入 `m_vertexs + offset`，并调用后端 `vertex_update` 钩子推送设备；资源尚未在图形层初始化时跳过钩子，首次绑定的 `vertex_init` 会整体上传最新内容。更新只改顶点数据：参与绘制的索引数量由绘制命令给出（`m_index_count` 是逻辑线程侧的绘制元数据，由网格所有者维护，保持至少 1）。粒子系统在录制更新前自行改写 `m_x/y/z_min/max` 修正视锥剔除用的包围盒。不参与绘制的系统（粒子）经 `je_graphic_branch_new_update_command_buffer` 以最小绘制组优先级录制更新；图形实现请求跳过本帧绘制时，已录制的更新经 `je_graphic_cmd_commit_updates` 仍然生效。

### 各后端的顶点缓冲

与纹理相同，网格在首次被图形线程使用时经 `vertex_create_blob` + `vertex_init` 创建后端对象；内容更新经命令提交时的 `vertex_update` 钩子重传（Vulkan 后端按帧龄选择可重命名槽位写入，隔离在飞帧）。以 OpenGL 后端为例：`vertex_init` 创建 VAO / VBO / EBO——顶点缓冲按 `GL_STATIC_DRAW` 上传，索引缓冲按 `m_index_capacity` 上传，每个属性按序号 i 设置 `glVertexAttribPointer`（INT32 用 `glVertexAttribIPointer`），图元类型映射为 `GL_LINE_STRIP` / `GL_TRIANGLES` / `GL_TRIANGLE_STRIP`；`vertex_update` 以 `GL_DYNAMIC_DRAW` 重新指定整个顶点缓冲（同时孤儿化旧存储，避免与 GPU 读取冲突），索引缓冲不重传；更新由命令缓冲区提交时在图形线程按序驱动（不再依赖 `m_modified` 标记）。顶点类资源在 OpenGL 后端没有可跨实例复用的 blob。

渲染系统按 `topology_of(m_type)` 选择 pipeline 拓扑，以 `je_graphic_cmd_draw_indexed(m_index_count)` 绘制；默认四边形由 `DefaultResources` 以 `{0, 1, 2, 3}` 索引和“位置 3 + UV 2 + 法线 3 + 切线 3”格式创建。视锥剔除的包围球半径取 `get_entity_size` 结果长度的一半：各轴为 `local_scale × 2 × max(|min|, |max|)`。

### 关键源码位置

| 内容 | 位置 |
| --- | --- |
| `je_gl_vertex_t`、布局与骨骼结构、C ABI | `include/jeecs/graphic.hpp` |
| C++ 包装 `graphic::vertex` | 同上 |
| 创建、Assimp 加载 | `src/jeecs_graphic_api_basic.cpp`（`_jegl_create_vertex_impl`、`je_graphic_vertex_load`） |
| 命令化内容更新 | `src/jeecs_graphic_cmd.cpp`（`je_graphic_cmd_update_vertex`） |
| Woolang 绑定 | `je/graphic.wo`、`je/ecs.wo`、`src/jeecs_graphic_woolang_api.cpp` |
| `Renderer::Shape` 组件 | `include/jeecs/components.hpp` |
| 默认四边形、拓扑映射、视锥剔除 | `src/jeecs_core_graphic_system.cpp` |
| 动态网格实践参考 | `src/jeecs_core_particle_system.cpp`、`src/jeecs_core_tilemap_system.cpp` |
| OpenGL 顶点缓冲实现 | `src/jeecs_graphic_api_impl_opengl3.cpp`（`vertex_init` / `vertex_update`） |
