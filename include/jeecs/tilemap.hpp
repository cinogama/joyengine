#pragma once

// ============================================================================
// jeecs/tilemap.hpp —— 瓦片地图：je_tilemap_* / je_tileset_* C API 与 jeecs::tilemap 只读文档查询 C++ 封装
// =============================================================================

#include "jeecs/prefix.hpp"
#include "jeecs/file.hpp"
#include "jeecs/math.hpp"
#include "jeecs/basic.hpp"

JE_FORCE_CAPI

// =============================================================================
// je_tilemap_* —— 瓦片地图 C API
// 地图(.je4tilemap)与图集(.je4tileset)文档的读写、瓦片/属性查询与自动图块支持。
// 文档以 je_tilemap_document_t* / je_tileset_document_t* 不透明实例指针（nullptr 为
// 无效值）标识，进程内全局缓存、按路径共享：编辑器经此修改文档后版本号自增，
// 各世界的 TilemapSystem 下一帧即重建渲染分片（未保存的修改也会实时同步到
// 世界实例）。
// 文档生命周期按引用计数管理：open/create 的调用方各持有 1 个引用，使用地图
// 的世界与引用图集的地图亦各占 1 个；je_tilemap_map_keep/je_tilemap_map_unload
//（图集侧同理）增减引用，归零即销毁实例（未保存的修改随之丢弃，此后一切
// 指针失效），下次按路径 open 自磁盘重读。
// 引用计数与缓存注册表是线程安全的：Woolang 为并发 GC，文档 gchandle 的
// 终结器会在 GC worker 线程上并行调用 unload（与编辑器线程、世界更新线程
// 并发），open/create 返回的引用与打开/注册在同一锁定区间内取得，不会在
// 取得引用前被并发销毁。文档内容（网格/图层等）仍为设计上的共享可变数据，
// 内容级并发读写不由此组 API 串行化。
// 瓦片值编码（网格/笔刷统一使用）：
//     0            空
//     v > 0        按原样绘制瓦片: (tileset_idx + 1) * 0x100000 + tile_id
//     v < 0        按自动拼接类型绘制: -((tileset_idx + 1) * 0x100000 + tile_id)
// 图集中的每个源纹理格都是一块瓦片（id 由图集源分配、跨保存稳定）；图集
// 打开时网格按纹理当前尺寸重整，纹理增删一行/一列即增删一行/一列瓦片
//（已有瓦片 id 不变）。"地形"不是独立实体，而是 je_tile_kind_t 非 PLAIN 的
// 瓦片。属性取值链：逐格覆盖 -> 瓦片默认值 -> 图集 schema 默认值。
// 属性值为类型化数据（je_property_type_t）：
// 设置/读取都须传入类型枚举并给出对应的数据指针，与实际存储类型不符时
// 返回 false。
// 瓦片的名称/通行性/自动拼接类型/顶点法线都是瓦片 id 上的稀疏元数据；法线
// 参与地图网格的顶点生成（默认 (0, 0, -1) 指向屏幕外侧），见
// je_tilemap_tile_normal / je_tilemap_tile_normal_set。
// =============================================================================

namespace jeecs { namespace Tilemap
{
    struct MapDocument;
    struct TilesetDocument;
}}

/*
je_tilemap_document_t [类型别名]
地图文档(.je4tilemap)的不透明实例指针，指向全局缓存、按路径共享的
地图文档对象
    * nullptr 是无效值；open/create 成功返回的指针携带 1 个文档引用，
      引用计数归零后实例销毁、指针随之失效，不得继续使用
请参见：
    je_tileset_document_t
    je_tilemap_map_open
*/
typedef jeecs::Tilemap::MapDocument je_tilemap_document_t;

/*
je_tileset_document_t [类型别名]
图集文档(.je4tileset)的不透明实例指针，指向全局缓存、按路径共享的
图集文档对象
    * nullptr 是无效值；open/create 成功返回的指针携带 1 个文档引用，
      引用计数归零后实例销毁、指针随之失效，不得继续使用
请参见：
    je_tilemap_document_t
    je_tilemap_tileset_open
*/
typedef jeecs::Tilemap::TilesetDocument je_tileset_document_t;

/*
je_layer_id_t [类型别名]
图层在地图文档内的下标，由 je_tilemap_layer_add 经 out_id 返回
    * 从 0 开始；remove/move 后其余图层下标会前移/重排，跨操作持有的
      下标需按 layer_count 重新校验
请参见：
    je_tilemap_document_t
    je_tilemap_layer_add
*/
typedef int32_t je_layer_id_t;

/*
je_source_id_t [类型别名]
源纹理在图集文档内的下标，由 je_tilemap_source_add 经 out_id 返回
    * 从 0 开始；remove 后其余源下标前移，跨操作持有的下标需按
      source_count 重新校验
请参见：
    je_tileset_document_t
    je_tilemap_source_add
*/
typedef int32_t je_source_id_t;

/*
je_tile_id_t [类型别名]
瓦片在图集文档内的 id。图集的每个源纹理格都是一块瓦片，id 由图集源
在添加时分配（按行优先连续分配）
    * 从 1 开始，0 是无效值
    * 删除源或纹理缩小裁掉格子后其 id 永不复用，跨保存/重载稳定；地图
      按 id 引用瓦片。图集打开时网格按纹理当前尺寸重整：新增格分配新
      id，已有格的 id 不变——因此 id 与格坐标不是固定公式关系
    * je_tilemap_tile_id_at 由格坐标反查 id，
      je_tilemap_tile_at_index / je_tilemap_tile_count 枚举全部瓦片
请参见：
    je_tileset_document_t
    je_tilemap_tile_id_at
*/
typedef int32_t je_tile_id_t;

/*
je_tile_kind_t [枚举]
瓦片的自动拼接类型（即"地形"行为），取值即 je_tilemap_tile_kind /
je_tilemap_tile_kind_set 的 kind 参数
    * JE_TILE_KIND_PLAIN        普通瓦片：整块绘制，不参与邻接解析
    * JE_TILE_KIND_RPGMAKER_XP  RPGMaker XP 式四象限（锚点起 2x3 条带）
    * JE_TILE_KIND_BLOB47       blob47（47 变体）
    * JE_TILE_KIND_RPGMAKER_VX  RPGMaker VX 式四象限（锚点起 2x3 块，右上
      2x2 为内部填充；孤块取锚点整块；与 RPG Maker VX Ace 渲染逐像素一致）
自动拼接类型是瓦片自身的属性：地图格存负值（见 je_tilemap_* 概述的编码）
时按该类型解析邻接变体；存正值时按原样绘制锚点格（PLAIN 两种符号等价）。
请参见：
    je_tilemap_tile_kind_set
*/
enum je_tile_kind_t
{
    JE_TILE_KIND_PLAIN = 0,
    JE_TILE_KIND_RPGMAKER_XP = 1,
    JE_TILE_KIND_BLOB47 = 2,
    JE_TILE_KIND_RPGMAKER_VX = 3,
};

/*
je_property_type_t [枚举]
瓦片地图属性值的类型，即属性接口 type 参数的取值。各类型经 void* 传递的
数据形态：
    * JE_PROPERTY_TYPE_BOOL   bool（1 字节）
    * JE_PROPERTY_TYPE_STRING const char*（设置）/ const char**（读取，
      指向文档内部存储，文档下次修改前有效）
    * JE_PROPERTY_TYPE_INT    int32_t
    * JE_PROPERTY_TYPE_FLOAT  float
    * JE_PROPERTY_TYPE_VEC2   float[2]
    * JE_PROPERTY_TYPE_VEC3   float[3]
    * JE_PROPERTY_TYPE_VEC4   float[4]
枚举值与内部 variant 存储的下标一一对应。
请参见：
    je_tilemap_cell_property_set
*/
enum je_property_type_t
{
    JE_PROPERTY_TYPE_BOOL = 0,
    JE_PROPERTY_TYPE_STRING = 1,
    JE_PROPERTY_TYPE_INT = 2,
    JE_PROPERTY_TYPE_FLOAT = 3,
    JE_PROPERTY_TYPE_VEC2 = 4,
    JE_PROPERTY_TYPE_VEC3 = 5,
    JE_PROPERTY_TYPE_VEC4 = 6,
};

/*
je_layer_self_shadow_t [枚举]
图层的 2D 光照自阴影行为（Light2D::SelfShadow 组件的挂载策略），即
je_tilemap_layer_selfshadow / je_tilemap_layer_selfshadow_set 的取值。
渲染分片是运行时生成的隐藏实体，无法逐个挂组件，故由图层属性驱动：
    * JE_LAYER_SELFSHADOW_OFF 禁用：该层分片不挂 SelfShadow 组件，
      图块正常接收其它图层遮挡体投下的阴影
    * JE_LAYER_SELFSHADOW_ON  启用（常开）：分片挂 SelfShadow 且
      auto_disable = false，无论光源高度如何，自阴影覆盖始终按 factor 生效
    * JE_LAYER_SELFSHADOW_AUTO 启用（自动）：分片挂 SelfShadow 且
      auto_disable = true，光源不高于该层时不追加自阴影覆盖（图块受光；
      仍会接收位于该层之前的遮挡阴影）
（覆盖强度 factor 经 je_tilemap_layer_selfshadow_factor_set 按层设置）
TopDown 光源（Light2D::TopDown）下的 AUTO 判定按格进行：分片同挂
Light2D::PerBlockSelfShadow（网格顶点携带块底边高度），格底边不低于光源
高度线时该格受光、否则写 factor——整张地图不会被实体原点一刀切。
（ON 模式与普通光源不受影响，仍为整实体判定）
瓦片分片带 PerBlockSelfShadow 标记。非 TopDown 光源下，分片自阴影与普通
精灵同款为“接收面”语义：自阴影 key 取图层 key 减半层偏移，按 painter key
覆盖写入——落在本层图块上的本层 BlockShadow 自遮挡被清除，且位于该层之后
（key 更大）的遮挡阴影一并被覆盖；只有位于该层之前（key 更小）的遮挡阴影
保留。TopDown 光源下自阴影改为逐块判定并按 MAX 合并：只加深阴影，不会
清除遮挡体已写入的阴影。
请参见：
    je_tilemap_layer_selfshadow_set
    je_tilemap_layer_selfshadow_factor_set
*/
enum je_layer_self_shadow_t
{
    JE_LAYER_SELFSHADOW_OFF = 0,
    JE_LAYER_SELFSHADOW_ON = 1,
    JE_LAYER_SELFSHADOW_AUTO = 2,
};

/*
je_layer_block_shadow_t [枚举]
图层的 2D 光照遮挡阴影行为（Light2D::BlockShadow 组件的挂载策略），即
je_tilemap_layer_blockshadow / je_tilemap_layer_blockshadow_set 的取值。
渲染分片是运行时生成的隐藏实体，无法逐个挂组件，故由图层属性驱动：
    * JE_LAYER_BLOCKSHADOW_OFF 禁用：该层分片不挂 BlockShadow 组件，
      瓦片不遮挡光线（不投阴影体）
    * JE_LAYER_BLOCKSHADOW_ON  启用（常开）：分片挂 BlockShadow 且
      auto_disable = false，无论光源高度如何，瓦片占据轮廓始终按 factor
      投射硬阴影（阴影体）
    * JE_LAYER_BLOCKSHADOW_AUTO 启用（自动）：分片挂 BlockShadow 且
      auto_disable = true，光源不高于该层时不投射遮挡阴影
遮挡轮廓由引擎按分块网格自动生成：该层有有效瓦片的格子视为实心，实心
与空心格的边界（跨分块判定，贪心合并共线段）构成遮挡轮廓，随网格编辑
重建。（阴影浓度 factor 经 je_tilemap_layer_blockshadow_factor_set 按层
设置）
TopDown 光源下，轮廓段的逐像素排序 key 取该段自身的最低边，并合并与本段
格子严格相邻（8 邻域）的其它开启 BlockShadow 图层墙段。贴合的跨层上下
墙段因此共享较低基座，不会因上段 Y 更高而被实体 SelfShadow 清除；隔开
2 格以上的墙不会把 key 拉低。
请参见：
    je_tilemap_layer_blockshadow_set
    je_tilemap_layer_blockshadow_factor_set
*/
enum je_layer_block_shadow_t
{
    JE_LAYER_BLOCKSHADOW_OFF = 0,
    JE_LAYER_BLOCKSHADOW_ON = 1,
    JE_LAYER_BLOCKSHADOW_AUTO = 2,
};

// ---- 地图文档 ----

/*
je_tilemap_map_open [基本接口]
打开（或取进程内缓存）指定路径的地图文档，返回文档实例指针
    * 路径不存在或无法解析时返回 nullptr
    * 每次成功调用为调用方持有 1 个文档引用（见 je_tilemap_map_unload）
请参见：
    je_tilemap_document_t
    je_tilemap_map_keep
*/
JE_API /* OPTIONAL: If path cannot be opened */ je_tilemap_document_t* je_tilemap_map_open(const char* path);

/*
je_tilemap_map_create [基本接口]
新建一个未保存的地图文档（单图层），返回文档实例指针
    * 尺寸不合法（<=0 或超过 65536）时返回 nullptr
    * 调用方持有 1 个文档引用（见 je_tilemap_map_unload）
请参见：
    je_tilemap_document_t
*/
JE_API /* OPTIONAL: If size is invalid */ je_tilemap_document_t* je_tilemap_map_create(int32_t w, int32_t h, int32_t tile_px);

/*
je_tilemap_map_path_bind [基本接口]
编辑器已把地图的 TOML 源写入 path（.je4tilemap）后，把该路径登记为
文档路径
    * 引擎不写盘、不生成/重建缓存：<path>.je4cache 在下次加载时按
      源文件 CRC64 自动失效并重建
    * 解除本文档的旧路径绑定并顶替 path 上的既有绑定
    * 文档须由调用方持有外部引用，否则返回 false
    * path 支持 @/ 与 !/ 前缀（与 je_tilemap_map_open 一致）
请参见：
    je_tilemap_map_open
*/
JE_API bool je_tilemap_map_path_bind(
    je_tilemap_document_t* map, const char* path);

/*
je_tilemap_map_reload [基本接口]
丢弃指定路径地图文档在缓存中的未保存修改，下次访问时从磁盘重读
*/
JE_API void je_tilemap_map_reload(const char* path);

/*
je_tilemap_map_size [基本接口]
取地图文档的尺寸信息，经出参返回 (宽, 高, 瓦片边长像素, 图层数)
    * 各出参均可传 nullptr 跳过
*/
JE_API void je_tilemap_map_size(je_tilemap_document_t* map,
    /* OPTIONAL */ int32_t* w, /* OPTIONAL */ int32_t* h, /* OPTIONAL */ int32_t* tile_px, /* OPTIONAL */ int32_t* layer_count);

/*
je_tilemap_map_path [基本接口]
取地图文档的登记路径，未保存（新建后未另存）时返回false
*/
JE_API bool je_tilemap_map_path(je_tilemap_document_t* map, const char** out_path);

/*
je_tilemap_map_version [基本接口]
取地图文档的修改版本号，任何经本组 API 的修改都会使其自增
*/
JE_API uint64_t je_tilemap_map_version(je_tilemap_document_t* map);

/*
je_tilemap_map_keep [基本接口]
地图文档引用计数 +1（文档一经加载默认长期驻留，引用计数是唯一的
卸载途径），返回增加后的计数
    * 引用来源：open/create 的调用方、使用该地图的各世界（各占 1）
    * 线程安全：可从任意线程（含 Woolang GC 终结器线程）调用
请参见：
    je_tilemap_map_unload
*/
JE_API int32_t je_tilemap_map_keep(je_tilemap_document_t* map);

/*
je_tilemap_map_unload [基本接口]
地图文档引用计数 -1（下限 0）；归零即销毁文档实例——指针立即失效，下次
按路径打开自磁盘重读（未保存的修改随之丢弃）
    * 返回减少后的计数（0 表示文档已被销毁或不可再卸载）
    * 线程安全：可从任意线程调用；Woolang 文档 gchandle 被并发 GC
      回收时即经终结器在 GC worker 线程上调用本函数
请参见：
    je_tilemap_map_keep
    je_tilemap_map_open
*/
JE_API int32_t je_tilemap_map_unload(je_tilemap_document_t* map);

/*
je_tilemap_loaded_map_count [基本接口]
返回进程内已注册的地图文档数量（含未保存的新建文档）
    * 文档实例销毁后其枚举槽位置空，计数不变但 je_tilemap_loaded_map
      在该下标返回 nullptr；计数也含仍被引用、但已被 je_tilemap_map_reload
      弃用的旧实例（其路径已清空、不再被任何世界使用）
请参见：
    je_tilemap_loaded_map
*/
JE_API int32_t je_tilemap_loaded_map_count(void);

/*
je_tilemap_loaded_map [基本接口]
按枚举下标取已注册地图文档的实例指针，与 je_tilemap_loaded_map_count 配套遍历
    * 下标从 0 起、小于 je_tilemap_loaded_map_count；越界或该槽位实例
      已销毁返回 nullptr
    * 枚举下标不是文档指针：文档指针稳定不变，而打开/新建文档会使后续
      枚举下标位移，遍历中不应再打开/新建文档
请参见：
    je_tilemap_loaded_map_count
*/
JE_API /* OPTIONAL: If index is out of range or the slot is empty */ je_tilemap_document_t* je_tilemap_loaded_map(int32_t index);

/*
je_tilemap_map_resize [基本接口]
调整地图文档的网格尺寸，越界格子的逐格属性覆盖将被丢弃
    * 新尺寸不合法（<=0 或超过 65536）时不做任何修改
*/
JE_API void je_tilemap_map_resize(je_tilemap_document_t* map, int32_t new_w, int32_t new_h);

// ---- 图层 ----

/*
je_tilemap_layer_add [基本接口]
向地图文档末尾追加一个图层，新图层下标（je_layer_id_t）经 out_id 返回
    * 失败时不写出下标并返回 false
请参见：
    je_layer_id_t
*/
JE_API bool je_tilemap_layer_add(je_tilemap_document_t* map, const char* name,
    je_layer_id_t* out_id);

/*
je_tilemap_layer_remove [基本接口]
移除指定下标的图层，并重排逐格属性键
    * 下标越界返回 false
*/
JE_API bool je_tilemap_layer_remove(je_tilemap_document_t* map, je_layer_id_t layer);

/*
je_tilemap_layer_move [基本接口]
把图层移动到新下标（自动钳制到 [0, 图层数-1]），并重排逐格属性键
*/
JE_API bool je_tilemap_layer_move(je_tilemap_document_t* map, je_layer_id_t layer, je_layer_id_t new_pos);

/*
je_tilemap_layer_get [基本接口]
取指定下标图层的信息，经出参返回 (名称, 是否可见, 是否锁定)
    * 各出参均可传 nullptr 跳过；下标越界时名称写出空串、其余写出 0
*/
JE_API void je_tilemap_layer_get(je_tilemap_document_t* map, je_layer_id_t layer,
    /* OPTIONAL */ const char** name, /* OPTIONAL */ int32_t* visible, /* OPTIONAL */ int32_t* locked);

/*
je_tilemap_layer_set [基本接口]
修改指定下标图层的名称、可见性与锁定状态
    * name 传 nullptr 表示保持名称不变
    * 下标越界时不做任何修改
*/
JE_API void je_tilemap_layer_set(je_tilemap_document_t* map, je_layer_id_t layer,
    /* OPTIONAL */ const char* name, int32_t visible, int32_t locked);

/*
je_tilemap_layer_z [基本接口]
取图层的世界 z 偏移（相对地图根实体；不同层须错开以避免深度冲突，
旧格式文件缺省为层下标 0,1,2...）
*/
JE_API float je_tilemap_layer_z(je_tilemap_document_t* map, je_layer_id_t layer);

/*
je_tilemap_layer_z_set [基本接口]
设置图层的世界 z 偏移
请参见：
    je_tilemap_layer_z
*/
JE_API void je_tilemap_layer_z_set(je_tilemap_document_t* map, je_layer_id_t layer, float z);

/*
je_tilemap_layer_rend_queue [基本接口]
取图层的渲染队列值（Renderer::Rendqueue）：同图各层分片按该值升序
绘制（画家算法，层间叠放的权威顺序）。新建图层缺省为 10000 + 层
下标（与场景实体常用的小队列值错开）；z 只在该层与写深度的场景
物体之间参与遮挡裁决
*/
JE_API int32_t je_tilemap_layer_rend_queue(je_tilemap_document_t* map, je_layer_id_t layer);

/*
je_tilemap_layer_rend_queue_set [基本接口]
设置图层的渲染队列值
请参见：
    je_tilemap_layer_rend_queue
*/
JE_API void je_tilemap_layer_rend_queue_set(je_tilemap_document_t* map, je_layer_id_t layer, int32_t rend_queue);

/*
je_tilemap_layer_selfshadow [基本接口]
取图层的 2D 光照自阴影行为（je_layer_self_shadow_t），下标越界时返回
JE_LAYER_SELFSHADOW_OFF
请参见：
    je_tilemap_layer_selfshadow_set
*/
JE_API je_layer_self_shadow_t je_tilemap_layer_selfshadow(je_tilemap_document_t* map, je_layer_id_t layer);

/*
je_tilemap_layer_selfshadow_set [基本接口]
设置图层的 2D 光照自阴影行为：OFF 时该层渲染分片移除 Light2D::SelfShadow
组件（不参与自阴影覆盖），ON/AUTO 时挂载该组件并分别固定 auto_disable 为
false / true；覆盖强度由 je_tilemap_layer_selfshadow_factor_set 设置
（缺省 1）。非 TopDown 光源下自阴影为接收面语义：按 painter key 清除本层
BlockShadow 落在本层图块上的自遮挡，同时抹掉位于该层之后的遮挡阴影，只
保留位于该层之前的阴影；TopDown 光源下按逐块 key 并 MAX 合并（只加深
阴影，不清除遮挡阴影）。
渲染分片由引擎生成，组件的挂载与摘除由 TilemapSystem 逐帧
对齐（走命令缓冲，下一帧迁移生效）；该设置随图层记录落盘
    * 下标越界时不做任何修改
请参见：
    je_layer_self_shadow_t
    je_tilemap_layer_selfshadow
    je_tilemap_layer_selfshadow_factor_set
*/
JE_API void je_tilemap_layer_selfshadow_set(je_tilemap_document_t* map, je_layer_id_t layer, je_layer_self_shadow_t selfshadow);

/*
je_tilemap_layer_selfshadow_factor [基本接口]
取图层的自阴影覆盖强度（写入分片 Light2D::SelfShadow::factor，即覆盖色
RGBA 乘子；仅图层模式为 ON/AUTO 时生效），下标越界时返回 1
请参见：
    je_tilemap_layer_selfshadow_factor_set
*/
JE_API float je_tilemap_layer_selfshadow_factor(je_tilemap_document_t* map, je_layer_id_t layer);

/*
je_tilemap_layer_selfshadow_factor_set [基本接口]
设置图层的自阴影覆盖强度：不做范围钳制（与 Light2D::SelfShadow::factor
组件本体同口径，常用 0..1）；强度与模式一样随图层记录落盘、由
TilemapSystem 逐帧对齐到分片组件
    * 下标越界时不做任何修改
请参见：
    je_tilemap_layer_selfshadow_factor
    je_tilemap_layer_selfshadow_set
*/
JE_API void je_tilemap_layer_selfshadow_factor_set(je_tilemap_document_t* map, je_layer_id_t layer, float factor);

/*
je_tilemap_layer_blockshadow [基本接口]
取图层的 2D 光照遮挡阴影行为（je_layer_block_shadow_t），下标越界时返回
JE_LAYER_BLOCKSHADOW_OFF
请参见：
    je_tilemap_layer_blockshadow_set
*/
JE_API je_layer_block_shadow_t je_tilemap_layer_blockshadow(je_tilemap_document_t* map, je_layer_id_t layer);

/*
je_tilemap_layer_blockshadow_set [基本接口]
设置图层的 2D 光照遮挡阴影行为：OFF 时该层渲染分片移除 Light2D::BlockShadow
组件（瓦片不遮挡光线），ON/AUTO 时挂载该组件并分别固定 auto_disable 为
false / true；阴影浓度由 je_tilemap_layer_blockshadow_factor_set 设置
（缺省 1）。遮挡轮廓（该层有效瓦片的占据边界，贪心合并共线段）由
TilemapSystem 自动生成并随网格编辑重建；该设置随图层记录落盘
    * 下标越界时不做任何修改
请参见：
    je_layer_block_shadow_t
    je_tilemap_layer_blockshadow
    je_tilemap_layer_blockshadow_factor_set
*/
JE_API void je_tilemap_layer_blockshadow_set(je_tilemap_document_t* map, je_layer_id_t layer, je_layer_block_shadow_t blockshadow);

/*
je_tilemap_layer_blockshadow_factor [基本接口]
取图层的遮挡阴影浓度（写入分片 Light2D::BlockShadow::factor，阴影体
浓度；仅图层模式为 ON/AUTO 时生效），下标越界时返回 1
请参见：
    je_tilemap_layer_blockshadow_factor_set
*/
JE_API float je_tilemap_layer_blockshadow_factor(je_tilemap_document_t* map, je_layer_id_t layer);

/*
je_tilemap_layer_blockshadow_factor_set [基本接口]
设置图层的遮挡阴影浓度：不做范围钳制（与 Light2D::BlockShadow::factor
组件本体同口径，常用 0..1）；浓度与模式一样随图层记录落盘、由
TilemapSystem 逐帧对齐到分片组件
    * 下标越界时不做任何修改
请参见：
    je_tilemap_layer_blockshadow_factor
    je_tilemap_layer_blockshadow_set
*/
JE_API void je_tilemap_layer_blockshadow_factor_set(je_tilemap_document_t* map, je_layer_id_t layer, float factor);

// ---- 图层着色器样式 ----
// 每层可直接持有一个专属着色器实例与若干附加纹理槽；图层样式是该层
// 分片渲染的唯一着色器来源（未配置样式的层分片着色器列表为空，不回落
// 地图根实体或内置默认）。uniform 值不经过瓦片地图——它们储存在着色器
// 实例本身，由实例的持有方直接设置与读取（graphic::shader::set_uniform），
// 渲染侧直接使用图层样式持有的实例，改动即时生效；保存文档时实例上的
// uniform 当前值随图层样式落盘（JE_ 引擎内置变量与 TEXTURE/矩阵类除外，
// 与 .je4entity 保存 Shaders 的口径一致），加载后写回新实例。
// 样式修改只自增图层样式版本（不走地图版本号），渲染侧做轻量对齐，
// 不触发网格重扫与顶点重建。
// 样式资源经下列不透明指针类型跨 C 边界传递（与 je_gl_frame_buffer_t 的
// je_gl_frame_buffer_attachment_t 同款姿势，C 侧不透明、实现侧按实际类型使用）：
//     je_tilemap_shader_ref_t  实为 jeecs::basic::resource<jeecs::graphic::shader>
//     je_tilemap_texture_ref_t 实为 jeecs::basic::resource<jeecs::graphic::texture2d>
typedef struct je_tilemap_shader_ref_t je_tilemap_shader_ref_t;
typedef struct je_tilemap_texture_ref_t je_tilemap_texture_ref_t;

/*
je_tilemap_layer_shader [基本接口]
取图层样式的着色器实例，返回新建引用副本（用 je_tilemap_shader_ref_free 释放）
    * 未指定样式时返回 nullptr
*/
JE_API /* OPTIONAL: If layer has no shader style */ je_tilemap_shader_ref_t* je_tilemap_layer_shader(je_tilemap_document_t* map,
    je_layer_id_t layer);

/*
je_tilemap_layer_shader_set [基本接口]
设置图层样式的着色器实例
    * shader 传 nullptr 表示清除样式（该层分片着色器列表被清空）
    * 样式共享持有传入实例：uniform 值设在实例上，改动下一帧生效
    * 下标越界时不做任何修改
*/
JE_API void je_tilemap_layer_shader_set(je_tilemap_document_t* map, je_layer_id_t layer,
    /* OPTIONAL */ je_tilemap_shader_ref_t* shader);

/*
je_tilemap_layer_texture_count [基本接口]
取图层样式配置的附加纹理数量
*/
JE_API int32_t je_tilemap_layer_texture_count(je_tilemap_document_t* map, je_layer_id_t layer);

/*
je_tilemap_layer_texture_set [基本接口]
设置（或替换/删除）图层样式某槽位的附加纹理
    * 槽 0 保留给图集源纹理，slot < 1 时返回 false
    * texture 传 nullptr 表示删除该槽位的绑定
    * 同槽位重复设置时替换旧值
*/
JE_API bool je_tilemap_layer_texture_set(je_tilemap_document_t* map, je_layer_id_t layer,
    int32_t slot, /* OPTIONAL */ je_tilemap_texture_ref_t* texture);

/*
je_tilemap_layer_texture_get [基本接口]
按序号（非槽位）取图层样式的附加纹理，经出参返回 (槽位, 纹理实例)
    * 纹理实例为新建引用副本（用 je_tilemap_texture_ref_free 释放）
    * 各出参均可传 nullptr 跳过；序号越界返回 false
*/
JE_API bool je_tilemap_layer_texture_get(je_tilemap_document_t* map, je_layer_id_t layer,
    int32_t index, /* OPTIONAL */ int32_t* slot, /* OPTIONAL */ je_tilemap_texture_ref_t** texture);

/*
je_tilemap_shader_ref_free [基本接口]
释放 je_tilemap_layer_shader 返回的着色器引用副本（传 nullptr 安全）
*/
JE_API void je_tilemap_shader_ref_free(/* OPTIONAL */ je_tilemap_shader_ref_t* shader);

/*
je_tilemap_texture_ref_free [基本接口]
释放 je_tilemap_layer_texture_get 出参返回的纹理引用副本（传 nullptr 安全）
*/
JE_API void je_tilemap_texture_ref_free(/* OPTIONAL */ je_tilemap_texture_ref_t* texture);

// ---- 瓦片读写 ----

/*
je_tilemap_tile_get [基本接口]
取指定图层某格的瓦片值（编码见本段说明），越界返回 0（空格值亦为 0）
*/
JE_API int32_t je_tilemap_tile_get(je_tilemap_document_t* map, je_layer_id_t layer,
    int32_t x, int32_t y);

/*
je_tilemap_layer_grid [基本接口]
取图层完整网格的只读视图（行优先，y 自上而下，长度 = width*height）
    * out_count 返回元素数量（可为 nullptr）
    * 图层无效时返回 nullptr
    * 指针在文档存活且网格未改动期间有效
    * 供编辑器序列化等需要遍历整层的场景，避免逐格调用 get_tile
*/
JE_API /* OPTIONAL: If layer is invalid */ const int32_t* je_tilemap_layer_grid(
    je_tilemap_document_t* map, je_layer_id_t layer, /* OPTIONAL */ int32_t* out_count);

/*
je_tilemap_tile_set [基本接口]
设置指定图层某格的瓦片值（编码见本段说明）
*/
JE_API void je_tilemap_tile_set(je_tilemap_document_t* map, je_layer_id_t layer,
    int32_t x, int32_t y, int32_t value);

/*
je_tilemap_tile_fill [基本接口]
把矩形区域内的格子批量设为同一瓦片值，供矩形/填充工具使用
    * 自动跳过越界格子；无实际变化时不自增版本号
*/
JE_API void je_tilemap_tile_fill(je_tilemap_document_t* map, je_layer_id_t layer,
    int32_t x, int32_t y, int32_t w, int32_t h, int32_t value);

// ---- 地图引用的图集 ----

/*
je_tilemap_tileset_count [基本接口]
取地图文档引用的图集数量
*/
JE_API int32_t je_tilemap_tileset_count(je_tilemap_document_t* map);

/*
je_tilemap_tileset_add [基本接口]
向地图文档登记一个图集引用，图集下标经 out_index 返回
    * 已引用同一路径时返回 true 且写出其下标；失败（路径为空）
      返回 false，不写出下标
    * 不要求图集此刻可解析，可先引用后保存图集
*/
JE_API bool je_tilemap_tileset_add(je_tilemap_document_t* map, const char* path,
    int32_t* out_index);

/*
je_tilemap_tileset_remove [基本接口]
移除地图文档对指定下标图集的引用，并清除引用该图集的所有格子
*/
JE_API bool je_tilemap_tileset_remove(je_tilemap_document_t* map, int32_t idx);

/*
je_tilemap_tileset_path [基本接口]
取地图文档引用的指定下标图集的路径，下标越界返回空串
*/
JE_API const char* je_tilemap_tileset_path(je_tilemap_document_t* map, int32_t idx);

// ---- 属性与查询 ----

/*
je_tilemap_cell_property_set [基本接口]
设置某格的属性逐格覆盖值（数据形态见 je_property_type_t）
    * value 传 nullptr 表示清除该格此属性的覆盖（此时忽略 type）
    * 该格已有不同类型的覆盖值时返回 false 且不修改
    * 坐标越界返回 false
请参见：
    je_property_type_t
*/
JE_API bool je_tilemap_cell_property_set(je_tilemap_document_t* map, je_layer_id_t layer,
    int32_t x, int32_t y, const char* name, je_property_type_t type,
    /* OPTIONAL */ const void* value);

/*
je_tilemap_cell_property_get [基本接口]
取某格的属性逐格覆盖值（不沿取值链解析），数据经 out_value 写回
（形态见 je_property_type_t）
    * 无覆盖或类型与实际存储不符返回 false
请参见：
    je_tilemap_property_get
    je_property_type_t
*/
JE_API bool je_tilemap_cell_property_get(je_tilemap_document_t* map, je_layer_id_t layer,
    int32_t x, int32_t y, const char* name, je_property_type_t type,
    /* OPTIONAL */ void* out_value);

/*
je_tilemap_property_get [基本接口]
按取值链（逐格覆盖 -> 瓦片默认值 -> 图集 schema 默认值）解析某格属性，
数据经 out_value 写回（形态见 je_property_type_t）
    * 无值或类型与实际存储不符返回 false
请参见：
    je_property_type_t
*/
JE_API bool je_tilemap_property_get(je_tilemap_document_t* map, je_layer_id_t layer,
    int32_t x, int32_t y, const char* name, je_property_type_t type,
    /* OPTIONAL */ void* out_value);

/*
je_tilemap_cell_property_entry_t [类型]
逐格属性覆盖条目的只读索引项，供编辑器序列化枚举
    * m_layer 为图层下标，m_x/m_y 为格子坐标
    * m_property_count 为该格覆盖的属性数量（名称/类型经
      je_tilemap_cell_property_entry_property_info 取得）
请参见：
    je_tilemap_cell_property_entries
*/
typedef struct je_tilemap_cell_property_entry_t
{
    je_layer_id_t m_layer;
    int32_t m_x;
    int32_t m_y;
    int32_t m_property_count;
} je_tilemap_cell_property_entry_t;

/*
je_tilemap_cell_property_entries [基本接口]
枚举文档中全部逐格属性覆盖条目（按 (layer, cell) 升序），返回条目总数
    * out_entries 容量不足时只写入前 capacity 项
    * out_entries 为 nullptr 或 capacity <= 0 时仅查询数量
请参见：
    je_tilemap_cell_property_entry_t
    je_tilemap_cell_property_entry_property_info
*/
JE_API int32_t je_tilemap_cell_property_entries(je_tilemap_document_t* map,
    /* OPTIONAL */ je_tilemap_cell_property_entry_t* out_entries, int32_t capacity);

/*
je_tilemap_cell_property_entry_property_info [基本接口]
取某格覆盖表中指定序号的属性名称与类型
    * index 越界或该格无覆盖时返回 false
请参见：
    je_tilemap_cell_property_entries
    je_property_type_t
*/
JE_API bool je_tilemap_cell_property_entry_property_info(
    je_tilemap_document_t* map, je_layer_id_t layer, int32_t x, int32_t y,
    int32_t index, /* OPTIONAL */ const char** name,
    /* OPTIONAL */ je_property_type_t* type);

/*
je_tilemap_is_walkable [基本接口]
取某格全图层合并的通行性（任一图层不可通行即不可通行），出界视为不可通行
    * 返回 1/0
*/
JE_API int32_t je_tilemap_is_walkable(je_tilemap_document_t* map, int32_t x, int32_t y);

/*
je_tilemap_is_walkable_on [基本接口]
取某格在指定图层上的通行性
    * 返回 1/0
请参见：
    je_tilemap_is_walkable
*/
JE_API int32_t je_tilemap_is_walkable_on(je_tilemap_document_t* map, je_layer_id_t layer,
    int32_t x, int32_t y);

/*
je_tilemap_cell_find [基本接口]
按属性值查找格子，返回命中数量；结果按文档缓存，经 je_tilemap_cell_find_get 逐个取出
    * layer < 0 时搜索全部图层
    * name 为 "walkable" 时按通行性匹配，type 须为 BOOL，value 指向 bool
请参见：
    je_tilemap_cell_find_get
    je_property_type_t
*/
JE_API int32_t je_tilemap_cell_find(je_tilemap_document_t* map, je_layer_id_t layer,
    const char* name, je_property_type_t type, const void* value);

/*
je_tilemap_cell_find_get [基本接口]
取出 je_tilemap_cell_find 结果中第 index 个格子，经出参返回其 (x, y)
    * 越界返回 false
请参见：
    je_tilemap_cell_find
*/
JE_API bool je_tilemap_cell_find_get(je_tilemap_document_t* map, int32_t index, int32_t* x, int32_t* y);

// ---- 图集文档(.je4tileset) ----

/*
je_tilemap_tileset_open [基本接口]
打开（或取进程内缓存）指定路径的图集文档，返回文档实例指针
    * 路径不存在或无法解析时返回 nullptr
    * 每次成功调用为调用方持有 1 个文档引用（见 je_tilemap_tileset_unload）
请参见：
    je_tileset_document_t
    je_tilemap_tileset_keep
*/
JE_API /* OPTIONAL: If path cannot be opened */ je_tileset_document_t* je_tilemap_tileset_open(const char* path);

/*
je_tilemap_tileset_create [基本接口]
新建一个未保存的图集文档，返回文档实例指针
    * 调用方持有 1 个文档引用（见 je_tilemap_tileset_unload）
请参见：
    je_tileset_document_t
*/
JE_API je_tileset_document_t* je_tilemap_tileset_create(const char* name);

/*
je_tilemap_tileset_path_bind [基本接口]
编辑器已把图集的 TOML 源写入 path（.je4tileset）后，把该路径登记为
文档路径
    * 引擎不写盘、不生成/重建缓存：<path>.je4cache 在下次加载时按
      源文件 CRC64 自动失效并重建
    * 解除本文档的旧路径绑定并顶替 path 上的既有绑定
    * 文档须由调用方持有外部引用，否则返回 false
    * path 支持 @/ 与 !/ 前缀（与 je_tilemap_tileset_open 一致）
请参见：
    je_tilemap_tileset_open
*/
JE_API bool je_tilemap_tileset_path_bind(
    je_tileset_document_t* tileset, const char* path);

/*
je_tilemap_tileset_reload [基本接口]
丢弃指定路径图集文档在缓存中的未保存修改，下次访问时从磁盘重读
*/
JE_API void je_tilemap_tileset_reload(const char* path);

/*
je_tilemap_tileset_version [基本接口]
取图集文档的修改版本号，任何经本组 API 的修改都会使其自增
*/
JE_API uint64_t je_tilemap_tileset_version(je_tileset_document_t* tileset);

/*
je_tilemap_tileset_keep [基本接口]
图集文档引用计数 +1，返回增加后的计数
    * 引用来源：open/create 的调用方、引用该图集的各地图（各占 1）
请参见：
    je_tilemap_tileset_unload
*/
JE_API int32_t je_tilemap_tileset_keep(je_tileset_document_t* tileset);

/*
je_tilemap_tileset_unload [基本接口]
图集文档引用计数 -1（下限 0）；归零即撤销注册表对该实例的持有——已无
引用方（引用它的地图等）时实例销毁、指针立即失效，下次按路径打开（或
地图解析）自磁盘重读（未保存的修改随之丢弃）
    * 返回减少后的计数（0 表示文档已被销毁或不可再卸载）
请参见：
    je_tilemap_tileset_keep
*/
JE_API int32_t je_tilemap_tileset_unload(je_tileset_document_t* tileset);

/*
je_tilemap_tileset_doc_path [基本接口]
取图集文档的登记路径，未保存时返回空串
*/
JE_API const char* je_tilemap_tileset_doc_path(je_tileset_document_t* tileset);

/*
je_tilemap_tileset_name_set [基本接口]
设置图集文档的名称
*/
JE_API void je_tilemap_tileset_name_set(je_tileset_document_t* tileset, const char* name);

/*
je_tilemap_tileset_name [基本接口]
取图集文档的名称
*/
JE_API const char* je_tilemap_tileset_name(je_tileset_document_t* tileset);

/*
je_tilemap_tileset_next_tile_id [基本接口]
取图集文档下一个可分配的瓦片 id（只增不减：删除源/裁掉格子后其 id
作废，不会被后续新增格复用）
    * 编辑器序列化 .je4tileset 时须原样保存，保证地图按 id 的引用不漂移
*/
JE_API int32_t je_tilemap_tileset_next_tile_id(je_tileset_document_t* tileset);

// ---- 图集源纹理 ----

/*
je_tilemap_source_add [基本接口]
向图集文档添加源纹理，tile_px 为瓦片边长（像素），网格数由纹理尺寸推得，
源下标（je_source_id_t）经 out_id 返回
    * 源的每个格都会分配稳定的瓦片 id（见 je_tile_id_t），无需再注册瓦片
    * 图集重新打开时网格按纹理当前尺寸重整（纹理增删一行/一列即增删
      一行/一列瓦片，已有瓦片 id 不变），见 je_tilemap_source_info
    * 已有同路径源时返回 true 且写出其下标；失败（纹理无法加载或参数不合法）
      返回 false，不写出下标
请参见：
    je_source_id_t
    je_tile_id_t
*/
JE_API bool je_tilemap_source_add(je_tileset_document_t* tileset,
    const char* texture_path, int32_t tile_px, je_source_id_t* out_id);

/*
je_tilemap_source_remove [基本接口]
移除指定下标的源纹理：该源格子的瓦片元数据随之丢弃，其余源下标前移
*/
JE_API bool je_tilemap_source_remove(je_tileset_document_t* tileset, je_source_id_t source_idx);

/*
je_tilemap_source_count [基本接口]
取图集文档的源纹理数量
*/
JE_API int32_t je_tilemap_source_count(je_tileset_document_t* tileset);

/*
je_tilemap_source_info [基本接口]
取指定下标源纹理的信息，经出参返回 (纹理路径, 瓦片边长, 横向格数, 纵向格数)
    * 行列数由纹理当前像素尺寸 / 瓦片边长推得（图集打开时重新测量），
      纹理增删一行/一列后随之增删
    * 下标越界返回 false
*/
JE_API bool je_tilemap_source_info(je_tileset_document_t* tileset, je_source_id_t source_idx,
    /* OPTIONAL */ const char** texture_path, /* OPTIONAL */ int32_t* tile_px, /* OPTIONAL */ int32_t* xcount, /* OPTIONAL */ int32_t* ycount);

/*
je_tilemap_source_tex_size [基本接口]
取指定源纹理的实测像素尺寸，经出参返回 (tex_w, tex_h)
    * 尺寸未知（尚未测量）时写 0；下标越界返回 false
    * 编辑器序列化 .je4tileset 时原样保存（加载时仍会按纹理重测）
*/
JE_API bool je_tilemap_source_tex_size(je_tileset_document_t* tileset,
    je_source_id_t source_idx,
    /* OPTIONAL */ int32_t* tex_w, /* OPTIONAL */ int32_t* tex_h);

/*
je_tilemap_source_cell_ids [基本接口]
取指定源纹理逐格瓦片 id 表的只读视图（行优先，长度 = xcount*ycount）
    * out_count 返回元素数量（可为 nullptr）
    * 下标越界返回 nullptr；指针在文档存活且源网格未改动期间有效
    * 供编辑器序列化原样保存（纹理尺寸变化后 id 与格坐标不是固定公式）
请参见：
    je_tilemap_tile_id_at
*/
JE_API /* OPTIONAL: If source_idx is invalid */ const int32_t*
    je_tilemap_source_cell_ids(je_tileset_document_t* tileset,
        je_source_id_t source_idx, /* OPTIONAL */ int32_t* out_count);

// ---- 图集源纹理附加层级 ----

/*
je_tilemap_source_extra_count [基本接口]
取指定源纹理的附加层级纹理（如法线图集）数量
    * 下标越界返回 0
*/
JE_API int32_t je_tilemap_source_extra_count(je_tileset_document_t* tileset,
    je_source_id_t source_idx);

/*
je_tilemap_source_extra_add [基本接口]
向源纹理添加附加层级纹理，pass 为其绑定通道 id（>= 1；通道 0 由主纹理/
反照率固定占用）。层级与主纹理共用同一瓦片网格（层号 = iy*xcount + ix
对齐，cell 尺寸由层级自身分辨率推得），渲染时按 pass 绑定为独立数组纹理
    * 同 (纹理路径, 通道) 幂等返回 true；同通道不同路径或同路径不同通道
      （换通道请用 je_tilemap_source_extra_pass_set）、纹理无法加载或
      尺寸与网格不符返回 false
请参见：
    je_tilemap_source_info
*/
JE_API bool je_tilemap_source_extra_add(je_tileset_document_t* tileset,
    je_source_id_t source_idx, const char* texture_path, int32_t pass);

/*
je_tilemap_source_extra_remove [基本接口]
移除源纹理的指定附加层级（按层级下标），其余层级下标前移
*/
JE_API bool je_tilemap_source_extra_remove(je_tileset_document_t* tileset,
    je_source_id_t source_idx, int32_t extra_idx);

/*
je_tilemap_source_extra_info [基本接口]
取指定附加层级的信息，经出参返回 (绑定通道 id, 纹理路径)
    * 下标越界返回 false
*/
JE_API bool je_tilemap_source_extra_info(je_tileset_document_t* tileset,
    je_source_id_t source_idx, int32_t extra_idx,
    /* OPTIONAL */ int32_t* pass, /* OPTIONAL */ const char** texture_path);

/*
je_tilemap_source_extra_tex_size [基本接口]
取指定附加层级的实测像素尺寸，经出参返回 (tex_w, tex_h)
    * 尺寸未知（尚未测量）时写 0；下标越界返回 false
    * 附加层级 cell 尺寸由该尺寸与主网格推得，编辑器序列化时原样保存
*/
JE_API bool je_tilemap_source_extra_tex_size(je_tileset_document_t* tileset,
    je_source_id_t source_idx, int32_t extra_idx,
    /* OPTIONAL */ int32_t* tex_w, /* OPTIONAL */ int32_t* tex_h);

/*
je_tilemap_source_extra_pass_set [基本接口]
修改附加层级的绑定通道 id（>= 1，同源内通道唯一），渲染侧逐帧对齐自动重绑
*/
JE_API bool je_tilemap_source_extra_pass_set(je_tileset_document_t* tileset,
    je_source_id_t source_idx, int32_t extra_idx, int32_t pass);

// ---- 瓦片表（图集每个格都是一块瓦片） ----

/*
je_tilemap_tile_count [基本接口]
取图集文档中的瓦片总数（各源纹理的格子数之和；每个格都是一块瓦片）
请参见：
    je_tilemap_tile_at_index
*/
JE_API int32_t je_tilemap_tile_count(je_tileset_document_t* tileset);

/*
je_tilemap_tile_at_index [基本接口]
按枚举下标取瓦片 id（按源的添加顺序、源内行优先展开）
    * 下标越界返回 false；配合 je_tilemap_tile_count 遍历全部瓦片
请参见：
    je_tilemap_tile_count
*/
JE_API bool je_tilemap_tile_at_index(je_tileset_document_t* tileset, int32_t index,
    je_tile_id_t* out_id);

/*
je_tilemap_tile_defs [基本接口]
枚举图集中带有稀疏元数据定义的瓦片 id（按 id 升序），返回总数
    * 未定义的瓦片按默认元数据（可通行、普通瓦片、无属性、空名称）处理
    * out_ids 容量不足时只写入前 capacity 项
    * out_ids 为 nullptr 或 capacity <= 0 时仅查询数量
    * 供编辑器序列化 .je4tileset 的 [[tiles]] 段
请参见：
    je_tilemap_tile_def_property_info
*/
JE_API int32_t je_tilemap_tile_defs(je_tileset_document_t* tileset,
    /* OPTIONAL */ je_tile_id_t* out_ids, int32_t capacity);

/*
je_tilemap_tile_def_property_count [基本接口]
取某瓦片定义的默认属性数量（该瓦片无定义返回 0）
*/
JE_API int32_t je_tilemap_tile_def_property_count(je_tileset_document_t* tileset,
    je_tile_id_t tile_id);

/*
je_tilemap_tile_def_property_info [基本接口]
取某瓦片定义中指定序号的属性名称与类型（按名称升序）
    * index 越界或该瓦片无定义返回 false
请参见：
    je_tilemap_tile_defs
    je_property_type_t
*/
JE_API bool je_tilemap_tile_def_property_info(je_tileset_document_t* tileset,
    je_tile_id_t tile_id, int32_t index,
    /* OPTIONAL */ const char** name, /* OPTIONAL */ je_property_type_t* type);

/*
je_tilemap_tile_id_at [基本接口]
取指定源纹理 (ix, iy) 格位的瓦片 id
    * 源/格位越界返回 false；每个格恒有瓦片，无需注册
请参见：
    je_tile_id_t
*/
JE_API bool je_tilemap_tile_id_at(je_tileset_document_t* tileset,
    je_source_id_t source_idx, int32_t ix, int32_t iy, je_tile_id_t* out_id);

/*
je_tilemap_tile_info [基本接口]
取指定 id 瓦片的信息，经出参返回 (源下标, 源内格位 ix/iy, 是否可通行)
    * id 不属于任何现存源格（越界/源已删除）返回 false
*/
JE_API bool je_tilemap_tile_info(je_tileset_document_t* tileset, je_tile_id_t tile_id,
    /* OPTIONAL */ je_source_id_t* source_idx, /* OPTIONAL */ int32_t* ix, /* OPTIONAL */ int32_t* iy, /* OPTIONAL */ int32_t* walkable);

/*
je_tilemap_tile_name [基本接口]
取指定 id 瓦片的名称；未命名或 id 无效返回空串
（指向文档内部存储，文档下次修改前有效）
请参见：
    je_tilemap_tile_name_set
*/
JE_API const char* je_tilemap_tile_name(je_tileset_document_t* tileset, je_tile_id_t tile_id);

/*
je_tilemap_tile_name_set [基本接口]
设置指定 id 瓦片的名称
    * id 无效返回 false
请参见：
    je_tilemap_tile_name
*/
JE_API bool je_tilemap_tile_name_set(je_tileset_document_t* tileset,
    je_tile_id_t tile_id, const char* name);

/*
je_tilemap_tile_walkable_set [基本接口]
设置指定 id 瓦片的默认通行性
*/
JE_API void je_tilemap_tile_walkable_set(je_tileset_document_t* tileset,
    je_tile_id_t tile_id, int32_t walkable);

// ---- 瓦片法线（参与地图网格顶点生成） ----

/*
je_tilemap_tile_normal [基本接口]
取指定 id 瓦片的顶点法线（局部空间单位向量），经出参返回 (x, y, z)
    * 默认 (0, 0, -1)，在引擎内指向屏幕外侧（朝向摄像机）
    * 出参均可传 nullptr 跳过；id 无效返回 false 且不写出参
请参见：
    je_tilemap_tile_normal_set
*/
JE_API bool je_tilemap_tile_normal(je_tileset_document_t* tileset, je_tile_id_t tile_id,
    /* OPTIONAL */ float* out_x, /* OPTIONAL */ float* out_y, /* OPTIONAL */ float* out_z);

/*
je_tilemap_tile_normal_set [基本接口]
设置指定 id 瓦片的顶点法线（按传入向量归一化后存储）
    * 地图网格生成时写入该瓦片的全部顶点；自动拼接格（四象限/变体）
      整格共享锚点瓦片的法线，取样出的变体瓦片不各取各的
    * (0,0,0) 或含非有限分量的向量非法，返回 false 且不修改
    * id 无效返回 false；设为默认 (0,0,-1) 即清除该瓦片的法线覆盖
请参见：
    je_tilemap_tile_normal
*/
JE_API bool je_tilemap_tile_normal_set(je_tileset_document_t* tileset,
    je_tile_id_t tile_id, float x, float y, float z);

// ---- 自动拼接（"地形"行为，是瓦片自身的属性） ----

/*
je_tilemap_tile_kind [基本接口]
取指定 id 瓦片的自动拼接类型；id 无效按 JE_TILE_KIND_PLAIN 返回
请参见：
    je_tile_kind_t
*/
JE_API je_tile_kind_t je_tilemap_tile_kind(je_tileset_document_t* tileset, je_tile_id_t tile_id);

/*
je_tilemap_tile_kind_set [基本接口]
设置指定 id 瓦片的自动拼接类型（kind 取值见 je_tile_kind_t）
    * id 无效或 kind 非法返回 false
    * 地图格存负值时按该类型解析邻接变体；PLAIN 即普通整块瓦片
请参见：
    je_tile_kind_t
*/
JE_API bool je_tilemap_tile_kind_set(je_tileset_document_t* tileset,
    je_tile_id_t tile_id, je_tile_kind_t kind);

/*
je_tilemap_tile_variant_count [基本接口]
取指定 id 瓦片的 blob47 显式变体数（0 = 按锚点起行优先连续取块的默认布局）
请参见：
    je_tilemap_tile_variant
*/
JE_API int32_t je_tilemap_tile_variant_count(je_tileset_document_t* tileset, je_tile_id_t tile_id);

/*
je_tilemap_tile_variant [基本接口]
按 47 变体序取指定 id 瓦片的显式变体瓦片 id
    * index 越界或未配置变体返回 false
请参见：
    je_tilemap_tile_variant_count
*/
JE_API bool je_tilemap_tile_variant(je_tileset_document_t* tileset, je_tile_id_t tile_id,
    int32_t index, je_tile_id_t* out_variant);

/*
je_tilemap_tile_variant_clear [基本接口]
清空指定 id 瓦片的 blob47 显式变体表；空表时按锚点行优先扫描 47 块
请参见：
    je_tilemap_tile_variant_add
*/
JE_API bool je_tilemap_tile_variant_clear(je_tileset_document_t* tileset, je_tile_id_t tile_id);

/*
je_tilemap_tile_variant_add [基本接口]
向指定 id 瓦片的 blob47 显式变体表追加一个变体瓦片 id（按 47 变体序）
    * 瓦片 id 或变体 id 无效返回 false
请参见：
    je_tilemap_tile_variant_clear
*/
JE_API bool je_tilemap_tile_variant_add(je_tileset_document_t* tileset,
    je_tile_id_t tile_id, je_tile_id_t variant_tile_id);

// ---- 属性 schema 与默认值 ----

/*
je_tilemap_property_add [基本接口]
向图集 schema 添加属性定义（数据形态见 je_property_type_t），属性下标经
out_index 返回
    * 重名、类型非法返回 false，不写出下标
    * default_value 传 nullptr 时按类型取零值（false/""/0/0.0/全 0 向量）
请参见：
    je_property_type_t
*/
JE_API bool je_tilemap_property_add(je_tileset_document_t* tileset,
    const char* name, je_property_type_t type,
    /* OPTIONAL */ const void* default_value, int32_t* out_index);

/*
je_tilemap_property_remove [基本接口]
按名称移除图集 schema 中的属性定义
*/
JE_API bool je_tilemap_property_remove(je_tileset_document_t* tileset, const char* name);

/*
je_tilemap_property_count [基本接口]
取图集 schema 中属性定义的数量
*/
JE_API int32_t je_tilemap_property_count(je_tileset_document_t* tileset);

/*
je_tilemap_property_info [基本接口]
取指定下标属性定义的信息，经出参返回 (名称, 类型)
    * 下标越界返回 false
请参见：
    je_property_type_t
*/
JE_API bool je_tilemap_property_info(je_tileset_document_t* tileset, int32_t index,
    /* OPTIONAL */ const char** name, /* OPTIONAL */ je_property_type_t* type);

/*
je_tilemap_property_default [基本接口]
按名称取图集 schema 中属性定义的默认值，数据经 out_value 写回
（形态见 je_property_type_t）
    * 无此属性或类型与定义不符返回 false
请参见：
    je_property_type_t
*/
JE_API bool je_tilemap_property_default(je_tileset_document_t* tileset,
    const char* name, je_property_type_t type, /* OPTIONAL */ void* out_value);

/*
je_tilemap_tile_property_set [基本接口]
设置指定 id 瓦片的属性默认值（数据形态见 je_property_type_t）
    * value 传 nullptr 表示清除该默认值（此时忽略 type）
    * 已有不同类型的默认值时返回 false 且不修改
*/
JE_API bool je_tilemap_tile_property_set(je_tileset_document_t* tileset, je_tile_id_t tile_id,
    const char* name, je_property_type_t type, /* OPTIONAL */ const void* value);

/*
je_tilemap_tile_property_get [基本接口]
取指定 id 瓦片的属性默认值，数据经 out_value 写回（形态见 je_property_type_t）
    * 无值或类型与实际存储不符返回 false
*/
JE_API bool je_tilemap_tile_property_get(je_tileset_document_t* tileset,
    je_tile_id_t tile_id, const char* name, je_property_type_t type,
    /* OPTIONAL */ void* out_value);

/*
je_tileset_cache_open_or_create [基本接口]
打开指定的瓦片集缓存文件；缓存缺失或失效时调用内置缓存生成器重建
    * 返回的句柄由调用方负责关闭（见 je_file_close）；无法生成时返回 nullptr
请参见：
    je_tilemap_cache_open_or_create
    je_file_close
*/
JE_API /* OPTIONAL */ je_file_t* je_tileset_cache_open_or_create(const char* path);

// ---- 单元格渲染信息（编辑器预览用，与实际渲染一致） ----

/*
je_tilemap_cell_quad_count [基本接口]
取某格渲染子四边形的数量：普通瓦片/整块自动图块为 1，四象限自动图块为 4，
空格为 0
请参见：
    je_tilemap_cell_quad
*/
JE_API int32_t je_tilemap_cell_quad_count(je_tilemap_document_t* map,
    je_layer_id_t layer, int32_t x, int32_t y);

/*
je_tilemap_cell_quad [基本接口]
取某格第 idx 个渲染子四边形，经出参返回 (源纹理路径, UV 矩形 u0/v0/u1/v1
引擎空间, 目标象限 qx/qy 于 qcols×qcols 网格、自左上角起)
    * idx 越界或格子为空返回 false
请参见：
    je_tilemap_cell_quad_count
*/
JE_API bool je_tilemap_cell_quad(je_tilemap_document_t* map, je_layer_id_t layer,
    int32_t x, int32_t y, int32_t idx,
    /* OPTIONAL */ const char** texture_path,
    /* OPTIONAL */ float* u0, /* OPTIONAL */ float* v0, /* OPTIONAL */ float* u1, /* OPTIONAL */ float* v1,
    /* OPTIONAL */ int32_t* qx, /* OPTIONAL */ int32_t* qy, /* OPTIONAL */ int32_t* qcols);

/*
je_tilemap_cache_open_or_create [基本接口]
打开指定的瓦片地图缓存文件；缓存缺失或失效时调用内置缓存生成器重建
    * 返回的句柄由调用方负责关闭（见 je_file_close）；无法生成时返回 nullptr
请参见：
    je_tileset_cache_open_or_create
    je_file_close
*/
JE_API /* OPTIONAL */ je_file_t* je_tilemap_cache_open_or_create(const char* path);
typedef je_tilemap_document_t je_TilemapDocument;
typedef je_tileset_document_t je_TilesetDocument;
typedef je_layer_id_t je_LayerId;
typedef je_source_id_t je_SourceId;
typedef je_tile_id_t je_TileId;
typedef je_tile_kind_t je_TileKind;
typedef je_property_type_t je_PropertyType;
typedef je_layer_self_shadow_t je_LayerSelfShadow;
typedef je_layer_block_shadow_t je_LayerBlockShadow;
typedef je_tilemap_shader_ref_t je_TilemapShaderRef;
typedef je_tilemap_texture_ref_t je_TilemapTextureRef;
typedef je_tilemap_cell_property_entry_t je_TilemapCellPropertyEntry;

JE_FORCE_CAPI_END
#ifdef __cplusplus
namespace jeecs
{
    /*
    tilemap [命名空间]
    瓦片地图/图集文档的只读查询封装，对应 je_tilemap_* C API 的读侧子集，
    供游戏运行时获取地图与图集信息（文档写入仍走 C API / 编辑器）。
    map_document / tileset_document 为按路径共享、引用计数管理的文档包装：
    open 返回 std::optional<basic::resource<...>>，包装构造时取得 1 个文档
    引用、析构时释放，resource 的各副本共享同一引用；句柄存活期间文档实例
    不会被销毁，可以安全持有，重复 open 同一路径只增加引用计数。引用
    计数操作线程安全，但文档内容（网格/图层等）是共享可变数据——与编辑器
    等并发写入方并行查询时需自行保证串行化。各名称/路径字符串指向文档内部
    存储，在文档下次被修改前有效。
    */
    namespace tilemap
    {
        /*
        tilemap::TILE_VALUE_BASE [常量]
        瓦片值编码基数：0 为空；v > 0 按原样绘制瓦片
        (tileset_index + 1) * TILE_VALUE_BASE + tile_id；v < 0 按瓦片的
        自动拼接类型绘制 -((tileset_index + 1) * TILE_VALUE_BASE + tile_id)
        */
        constexpr int32_t TILE_VALUE_BASE = 0x100000;

        /*
        tilemap::cell [结构体]
        解码后的格子瓦片值，由 tilemap::decode_cell 生成（空格经 nullopt 表达）
        */
        struct cell
        {
            int32_t tileset_index;  // 所属图集在地图中的下标（0 起）
            bool is_auto;           // 是否按瓦片的自动拼接类型绘制（值为负）
            int32_t tile_id;        // 瓦片 id（je_tile_id_t，从 1 起）
        };

        /*
        tilemap::decode_cell [函数]
        把 C API 的原始瓦片值（编码见 tilemap::TILE_VALUE_BASE）解码为
        tilemap::cell；空格（raw_value == 0）返回 nullopt
        */
        constexpr std::optional<cell> decode_cell(int32_t raw_value) noexcept
        {
            if (raw_value == 0)
                return std::nullopt;

            const uint32_t magnitude =
                raw_value > 0 ? (uint32_t)raw_value : (uint32_t)(-(int64_t)raw_value);
            cell result{};
            result.tileset_index = (int32_t)(magnitude / TILE_VALUE_BASE - 1);
            result.is_auto = raw_value < 0;
            result.tile_id = (int32_t)(magnitude % TILE_VALUE_BASE);
            return result;
        }

        /*
        tilemap::map_size [结构体]
        地图尺寸信息
        */
        struct map_size
        {
            int32_t w;       // 横向格数
            int32_t h;       // 纵向格数
            int32_t tile_px; // 瓦片边长（像素）
        };

        /*
        tilemap::layer_info [结构体]
        图层信息（name 指向文档内部存储，文档下次修改前有效）
        */
        struct layer_info
        {
            const char* name;
            bool visible;
            bool locked;
            float z; // 相对地图根实体的世界 z 偏移
            int32_t rend_queue; // 渲染队列值（同图各层按该值升序绘制）
        };

        /*
        tilemap::cell_coord [结构体]
        格子坐标（find_cells 的查询单元）
        */
        struct cell_coord
        {
            int32_t x;
            int32_t y;
        };

        /*
        tilemap::source_info [结构体]
        图集源纹理信息（texture_path 指向文档内部存储，文档下次修改前有效）
        */
        struct source_info
        {
            const char* texture_path;
            int32_t tile_px; // 瓦片边长（像素）
            int32_t xcount;  // 横向格数
            int32_t ycount;  // 纵向格数
        };

        /*
        tilemap::tile_info [结构体]
        瓦片信息（图集侧；name 指向文档内部存储，文档下次修改前有效）
        */
        struct tile_info
        {
            const char* name;      // 瓦片名称（未命名为空串）
            int32_t source_index;  // 所属源纹理下标
            int32_t ix;            // 源纹理内格位（列）
            int32_t iy;            // 源纹理内格位（行）
            bool walkable;         // 默认通行性
            je_tile_kind_t kind;      // 自动拼接类型（PLAIN = 普通瓦片）
            int32_t variant_count; // blob47 显式变体数
            math::vec3 normal;     // 顶点法线（局部空间；默认 (0,0,-1)）
        };

        /*
        tilemap::property_def [结构体]
        图集 schema 中的属性定义（name 指向文档内部存储，文档下次修改前有效）
        */
        struct property_def
        {
            const char* name;
            je_property_type_t type;
        };

        /*
        tilemap::detail [命名空间]
        property_traits 的实现细节，不应被外部使用
        */
        namespace detail
        {
            // find_cells 的匹配值按值地址传参（字符串除外，见 const char* 特化）
            template <typename T>
            struct value_by_address
            {
                static const void* value_ptr(const T& value) noexcept
                {
                    return &value;
                }
            };
        }

        /*
        tilemap::property_traits [类模板]
        属性值 C++ 类型（bool / const char* / int32_t / float / math::vec2/3/4）
        与 je_property_type_t 的映射。const char* 读取到的指针指向文档内部存储，
        文档下次修改前有效
        */
        template <typename T>
        struct property_traits
        {
            static_assert(sizeof(T) == 0,
                "unsupported property value type, use one of: "
                "bool / const char* / int32_t / float / math::vec2 / math::vec3 / math::vec4");
        };

        template <>
        struct property_traits<bool> : detail::value_by_address<bool>
        {
            static constexpr je_property_type_t TYPE = JE_PROPERTY_TYPE_BOOL;
            using buffer_t = bool;
            static std::optional<bool> take(const buffer_t& buffer) noexcept
            {
                return buffer;
            }
        };

        template <>
        struct property_traits<const char*>
        {
            static constexpr je_property_type_t TYPE = JE_PROPERTY_TYPE_STRING;
            using buffer_t = const char*;
            static std::optional<const char*> take(const buffer_t& buffer) noexcept
            {
                return buffer != nullptr ? buffer : std::optional<const char*>{};
            }
            // 字符串匹配值按 const char* 直传
            static const void* value_ptr(const char* const& value) noexcept
            {
                return value;
            }
        };

        template <>
        struct property_traits<int32_t> : detail::value_by_address<int32_t>
        {
            static constexpr je_property_type_t TYPE = JE_PROPERTY_TYPE_INT;
            using buffer_t = int32_t;
            static std::optional<int32_t> take(const buffer_t& buffer) noexcept
            {
                return buffer;
            }
        };

        template <>
        struct property_traits<float> : detail::value_by_address<float>
        {
            static constexpr je_property_type_t TYPE = JE_PROPERTY_TYPE_FLOAT;
            using buffer_t = float;
            static std::optional<float> take(const buffer_t& buffer) noexcept
            {
                return buffer;
            }
        };

        template <>
        struct property_traits<math::vec2> : detail::value_by_address<math::vec2>
        {
            static constexpr je_property_type_t TYPE = JE_PROPERTY_TYPE_VEC2;
            using buffer_t = float[2];
            static std::optional<math::vec2> take(const buffer_t& buffer) noexcept
            {
                return math::vec2(buffer[0], buffer[1]);
            }
        };

        template <>
        struct property_traits<math::vec3> : detail::value_by_address<math::vec3>
        {
            static constexpr je_property_type_t TYPE = JE_PROPERTY_TYPE_VEC3;
            using buffer_t = float[3];
            static std::optional<math::vec3> take(const buffer_t& buffer) noexcept
            {
                return math::vec3(buffer[0], buffer[1], buffer[2]);
            }
        };

        template <>
        struct property_traits<math::vec4> : detail::value_by_address<math::vec4>
        {
            static constexpr je_property_type_t TYPE = JE_PROPERTY_TYPE_VEC4;
            using buffer_t = float[4];
            static std::optional<math::vec4> take(const buffer_t& buffer) noexcept
            {
                return math::vec4(buffer[0], buffer[1], buffer[2], buffer[3]);
            }
        };

        /*
        tilemap::tileset_document [类]
        图集文档(.je4tileset)的只读查询包装（经 basic::resource 共享持有）：
        包装构造时取得 1 个文档引用，析构时释放，resource 的各副本共享同一
        引用，最后一个副本释放时卸载文档
        */
        class tileset_document
        {
            JECS_DISABLE_MOVE_AND_COPY(tileset_document);

            je_tileset_document_t* _m_doc;

            tileset_document(je_tileset_document_t* doc)
                : _m_doc(doc)
            {
                assert(_m_doc != nullptr);
            }

        public:
            ~tileset_document()
            {
                je_tilemap_tileset_unload(_m_doc);
            }

            /*
            tilemap::tileset_document::open [静态函数]
            打开（或取进程内缓存）指定路径的图集文档，失败返回 std::nullopt
            */
            static std::optional<basic::resource<tileset_document>> open(const char* path)
            {
                je_tileset_document_t* doc = je_tilemap_tileset_open(path);
                if (doc == nullptr)
                    return std::nullopt;
                return basic::resource<tileset_document>(new tileset_document(doc));
            }

            // 底层 C API 文档指针（逃生口）
            je_tileset_document_t* handle() const noexcept
            {
                return _m_doc;
            }

            // 图集名称（文档内部存储，文档下次修改前有效）
            const char* name() const noexcept
            {
                assert(_m_doc != nullptr);
                return je_tilemap_tileset_name(_m_doc);
            }
            // 登记路径，未保存（新建后未另存）时为空串
            const char* path() const noexcept
            {
                assert(_m_doc != nullptr);
                return je_tilemap_tileset_doc_path(_m_doc);
            }
            // 修改版本号（任何经 je_tilemap_* 的修改都会自增，可用于变更检测）
            uint64_t version() const noexcept
            {
                assert(_m_doc != nullptr);
                return je_tilemap_tileset_version(_m_doc);
            }

            // ---- 源纹理 ----

            // 源纹理数量
            int32_t source_count() const noexcept
            {
                assert(_m_doc != nullptr);
                return je_tilemap_source_count(_m_doc);
            }
            // 取指定下标源纹理信息；下标越界返回 std::nullopt
            std::optional<source_info> source(je_source_id_t index) const
            {
                assert(_m_doc != nullptr);
                source_info info;
                if (!je_tilemap_source_info(_m_doc, index,
                        &info.texture_path, &info.tile_px, &info.xcount, &info.ycount))
                    return std::nullopt;
                return info;
            }

            // ---- 瓦片（图集每个格都是一块瓦片；id 见 tilemap::cell::tile_id）----

            // 瓦片总数（各源纹理格子数之和）
            int32_t tile_count() const noexcept
            {
                assert(_m_doc != nullptr);
                return je_tilemap_tile_count(_m_doc);
            }
            // 按枚举下标取瓦片 id；越界返回 std::nullopt
            std::optional<je_tile_id_t> tile_at_index(int32_t index) const
            {
                assert(_m_doc != nullptr);
                je_tile_id_t id = 0;
                if (!je_tilemap_tile_at_index(_m_doc, index, &id))
                    return std::nullopt;
                return id;
            }
            // 取指定源纹理格位的瓦片 id；越界返回 std::nullopt
            std::optional<je_tile_id_t> tile_id_at(je_source_id_t source_index,
                int32_t ix, int32_t iy) const
            {
                assert(_m_doc != nullptr);
                je_tile_id_t id = 0;
                if (!je_tilemap_tile_id_at(_m_doc, source_index, ix, iy, &id))
                    return std::nullopt;
                return id;
            }
            // 取指定 id 瓦片信息；id 不属于任何现存源格返回 std::nullopt
            std::optional<tile_info> tile(je_tile_id_t id) const
            {
                assert(_m_doc != nullptr);
                tile_info info;
                int32_t walkable = 0;
                if (!je_tilemap_tile_info(_m_doc, id,
                        &info.source_index, &info.ix, &info.iy, &walkable))
                    return std::nullopt;
                info.name = je_tilemap_tile_name(_m_doc, id);
                info.walkable = walkable != 0;
                info.kind = je_tilemap_tile_kind(_m_doc, id);
                info.variant_count = je_tilemap_tile_variant_count(_m_doc, id);
                float nx = 0.f, ny = 0.f, nz = -1.f;
                je_tilemap_tile_normal(_m_doc, id, &nx, &ny, &nz);
                info.normal = math::vec3(nx, ny, nz);
                return info;
            }

            // ---- 属性 schema ----

            // schema 中属性定义的数量
            int32_t property_count() const noexcept
            {
                assert(_m_doc != nullptr);
                return je_tilemap_property_count(_m_doc);
            }
            // 取 schema 中指定下标的属性定义；越界返回 std::nullopt
            std::optional<property_def> property(int32_t index) const
            {
                assert(_m_doc != nullptr);
                property_def def;
                if (!je_tilemap_property_info(_m_doc, index, &def.name, &def.type))
                    return std::nullopt;
                return def;
            }
            /*
            按名称取 schema 属性默认值；T 的取值见 tilemap::property_traits，
            无此属性或类型与定义不符返回 std::nullopt
            */
            template <typename T>
            std::optional<T> property_default(const char* name) const
            {
                assert(_m_doc != nullptr);
                typename property_traits<T>::buffer_t buffer{};
                if (!je_tilemap_property_default(_m_doc, name, property_traits<T>::TYPE, &buffer))
                    return std::nullopt;
                return property_traits<T>::take(buffer);
            }
        };

        /*
        tilemap::map_document [类]
        地图文档(.je4tilemap)的只读查询包装（经 basic::resource 共享持有）：
        包装构造时取得 1 个文档引用，析构时释放，resource 的各副本共享同一
        引用，最后一个副本释放时卸载文档
        */
        class map_document
        {
            JECS_DISABLE_MOVE_AND_COPY(map_document);

            je_tilemap_document_t* _m_doc;

            map_document(je_tilemap_document_t* doc)
                : _m_doc(doc)
            {
                assert(_m_doc != nullptr);
            }

        public:
            ~map_document()
            {
                je_tilemap_map_unload(_m_doc);
            }

            /*
            tilemap::map_document::open [静态函数]
            打开（或取进程内缓存）指定路径的地图文档，失败返回 std::nullopt
            */
            static std::optional<basic::resource<map_document>> open(const char* path)
            {
                je_tilemap_document_t* doc = je_tilemap_map_open(path);
                if (doc == nullptr)
                    return std::nullopt;
                return basic::resource<map_document>(new map_document(doc));
            }

            // 底层 C API 文档指针（逃生口）
            je_tilemap_document_t* handle() const noexcept
            {
                return _m_doc;
            }

            // ---- 地图信息 ----

            // 尺寸信息（宽/高格数与瓦片边长像素）
            map_size size() const noexcept
            {
                assert(_m_doc != nullptr);
                map_size result;
                je_tilemap_map_size(_m_doc, &result.w, &result.h, &result.tile_px, nullptr);
                return result;
            }
            // 图层数量
            int32_t layer_count() const noexcept
            {
                assert(_m_doc != nullptr);
                int32_t layer_count = 0;
                je_tilemap_map_size(_m_doc, nullptr, nullptr, nullptr, &layer_count);
                return layer_count;
            }
            // 登记路径，未保存（新建后未另存）时为空串
            const char* path() const noexcept
            {
                assert(_m_doc != nullptr);
                const char* path = nullptr;
                if (!je_tilemap_map_path(_m_doc, &path))
                    return "";
                return path != nullptr ? path : "";
            }
            // 修改版本号（任何经 je_tilemap_* 的修改都会自增，可用于变更检测）
            uint64_t version() const noexcept
            {
                assert(_m_doc != nullptr);
                return je_tilemap_map_version(_m_doc);
            }

            // ---- 图层 ----

            // 取指定下标图层信息；越界返回 std::nullopt
            std::optional<layer_info> layer(je_layer_id_t index) const
            {
                assert(_m_doc != nullptr);
                if (index < 0 || index >= layer_count())
                    return std::nullopt;
                layer_info info;
                int32_t visible = 0, locked = 0;
                je_tilemap_layer_get(_m_doc, index, &info.name, &visible, &locked);
                info.visible = visible != 0;
                info.locked = locked != 0;
                info.z = je_tilemap_layer_z(_m_doc, index);
                info.rend_queue = je_tilemap_layer_rend_queue(_m_doc, index);
                return info;
            }

            // ---- 格子查询 ----

            // 取指定图层某格的瓦片值（越界与空格均为 nullopt）
            std::optional<cell> tile(je_layer_id_t layer_index, int32_t x, int32_t y) const noexcept
            {
                assert(_m_doc != nullptr);
                return decode_cell(je_tilemap_tile_get(_m_doc, layer_index, x, y));
            }
            // 某格全图层合并的通行性（任一图层不可通行即不可通行，出界视为不可通行）
            bool walkable(int32_t x, int32_t y) const noexcept
            {
                assert(_m_doc != nullptr);
                return je_tilemap_is_walkable(_m_doc, x, y) != 0;
            }
            // 某格在指定图层上的通行性
            bool walkable(je_layer_id_t layer_index, int32_t x, int32_t y) const noexcept
            {
                assert(_m_doc != nullptr);
                return je_tilemap_is_walkable_on(_m_doc, layer_index, x, y) != 0;
            }
            /*
            按取值链（逐格覆盖 -> 瓦片默认值 -> 图集 schema 默认值）解析
            某格属性；T 的取值见 tilemap::property_traits，无值或类型与实际
            存储不符返回 std::nullopt
            */
            template <typename T>
            std::optional<T> get_property(je_layer_id_t layer_index, int32_t x, int32_t y,
                const char* name) const
            {
                assert(_m_doc != nullptr);
                typename property_traits<T>::buffer_t buffer{};
                if (!je_tilemap_property_get(_m_doc, layer_index, x, y, name,
                        property_traits<T>::TYPE, &buffer))
                    return std::nullopt;
                return property_traits<T>::take(buffer);
            }
            /*
            按属性值查找格子，返回全部命中坐标；layer_index < 0 时搜索全部图层。
            T 的取值同 get_property。name 为 "walkable" 时按通行性匹配，
            此时 T 须为 bool
            */
            template <typename T>
            std::vector<cell_coord> find_cells(je_layer_id_t layer_index, const char* name,
                const T& value) const
            {
                assert(_m_doc != nullptr);
                std::vector<cell_coord> result;
                const int32_t count = je_tilemap_cell_find(_m_doc, layer_index, name,
                    property_traits<T>::TYPE, property_traits<T>::value_ptr(value));
                for (int32_t i = 0; i < count; ++i)
                {
                    cell_coord coord;
                    if (je_tilemap_cell_find_get(_m_doc, i, &coord.x, &coord.y))
                        result.push_back(coord);
                }
                return result;
            }
            // find_cells 的字符串匹配重载（按 const char* 内容匹配）
            std::vector<cell_coord> find_cells(je_layer_id_t layer_index, const char* name,
                const char* value) const
            {
                return find_cells<const char*>(layer_index, name, value);
            }

            // ---- 地图引用的图集 ----

            // 地图引用的图集数量
            int32_t tileset_count() const noexcept
            {
                assert(_m_doc != nullptr);
                return je_tilemap_tileset_count(_m_doc);
            }
            // 取指定下标图集的路径（越界为空串；文档内部存储，文档下次修改前有效）
            const char* tileset_path(int32_t index) const noexcept
            {
                assert(_m_doc != nullptr);
                return je_tilemap_tileset_path(_m_doc, index);
            }
            // 打开指定下标引用的图集文档（路径为空或打开失败返回 std::nullopt）
            std::optional<basic::resource<tileset_document>> open_tileset(int32_t index) const
            {
                assert(_m_doc != nullptr);
                const char* path = je_tilemap_tileset_path(_m_doc, index);
                if (path == nullptr || *path == '\0')
                    return std::nullopt;
                return tileset_document::open(path);
            }
        };
    }
}
#endif // __cplusplus
