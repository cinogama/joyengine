#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/components.hpp"
#include "jeecs/ecs.hpp"
#include "jeecs/game.hpp"
#include "jeecs_core_tilemap_model.hpp"

#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <optional>
#include <memory>

/*
TilemapSystem [系统] 与 je_tilemap_* C API 实现
瓦片地图核心的渲染侧：把地图文档实例化为"图层 × 分块 × 图集源"的
渲染子实体。文档模型、序列化与自动图块解析见
jeecs_core_tilemap_model.hpp；本模块（本头文件声明 + 对应 cpp 的实现）
只做三件事：

  * TilemapSystem —— Update 相位把文档变化翻译成渲染分片的增删与
    顶点重写（渲染之前完成数据构建）；根实体持有 Tilemap::Wall 时，
    另按不可通行掩码生成/回收静态 Physics2D 碰撞体子实体；
  * je_tilemap_* C API —— 文档读写与运行时查询的 C 边界
    （声明与 JE_API 导出标记统一见 include/jeecs.hpp）；
  * wojeapi_tilemap_* —— 上述 C API 的 Woolang 绑定胶水
    （je/tilemap.wo 经 extern 声明引用，机械对应，勿手写签名）。

工作方式：
  * 地图/图集文档进程内全局缓存（documents 注册表），按路径共享；
    编辑器经 je_tilemap_* 修改文档后 version 自增，各世界的
    TilemapSystem 在 Update 相位比对合并版本戳（map_stamp，含图集
    实例身份）并重建受影响的渲染分片 —— 未保存的编辑也会实时同步
    到所有世界实例。版本戳未变化的帧只做分片着色器/比例尺对齐，
    不再扫描网格。
  * 渲染分片（RenderPart）为地图根实体的子实体，每个分片持有
    32×32 格、单一图集源纹理的动态顶点缓冲（创建时一次分配到容量
    上限、静态角点索引，之后整体重写并收缩激活索引数，与粒子系统
    同款）。分片实体带 Editor::Invisible：不显示于实体列表、不参与
    世界保存，加载/重载后由本系统自动重建。
  * 实体句柄不跨帧缓存（架构约束）：碎片整理与组件增删都会移动
    实体，隔帧 je_game_entity_t 的组件访问会静默返回空。跨帧簿记只存
    纯数据（版本戳/掩码哈希）；分片与墙体实体每帧由 RenderPart/
    WallPart 查询重新定位，当帧句柄当帧用。
  * 图层自阴影（je_layer_self_shadow_t）：ON/AUTO 模式的层为其全部分片挂
    Light2D::SelfShadow（auto_disable 分别固定 false/true，factor 取
    图层强度 selfshadow_factor，缺省 1），OFF 模式的层不挂该组件；同挂
    Light2D::PerBlockSelfShadow。非 TopDown 光源下自阴影为“接收面”
    语义（与普通精灵同款）：surface key 取图层 key 减半层偏移覆盖写入，
    清除本层 BlockShadow 落在本层图块上的自遮挡，同时覆盖位于该层之后
    （key 更大）的遮挡阴影，只保留位于该层之前的阴影；TopDown 光源下
    按网格顶点携带的块底边逐格判高，自阴影按 MAX 合并（只加深阴影）。
    与着色器/纹理/比例尺同款每帧对齐（组件增删走命令缓冲，下一帧迁移
    生效）。
  * 图层遮挡阴影（je_layer_block_shadow_t）：ON/AUTO 模式的层为其全部分片
    挂 Light2D::BlockShadow（auto_disable 分别固定 false/true，factor 取
    图层强度 blockshadow_factor，缺省 1）。遮挡轮廓由本系统按分块生成：
    该层有有效瓦片的格子视为实心，实心与空心格的边界（跨分块判定，
    贪心合并共线段）以阴影体条带（TRIANGLESTRIP，逐顶点挤出因子）
    直接写入组件的 m_block_mesh，随版本戳（网格编辑）重建；同分块的
    多个分片各持相同轮廓，阴影缓冲 MAX 混合下重复绘制幂等。挂载由
    sync_part_blockshadow 统一完成（add_component 走命令缓冲返回暂存
    实例，创建当帧读不到池内组件，故不在 create_part_entity 挂载）。
  * 碰撞体墙（WallPart）同为根实体的子实体：根实体持有 Tilemap::Wall
    时，把不可通行格（任一图层不可通行即墙，与 je_tilemap_is_walkable
    同一语义）贪心合并为矩形，每矩形一个静态 Physics2D::Collider::Box，
    Rigidbody::layerid 取自 Wall::layerid。掩码哈希未变化的版本戳
    （纯视觉/样式编辑）不重建碰撞体，避免物理世界无谓抖动。
    Wall::block_empty 开启严格模式：无图块的空格与地图外缘（四周向外
    一圈）一并并入墙体，只有可通行图块覆盖的格子可通过。
    Wall::restitution / Wall::friction 为墙体弹性/摩擦力系数，写入各
    碰撞体实体的 Physics2D::Restitution / Friction 并每帧跟随同步。
  * 自动图块在网格中只存瓦片 id（负值 = 按自动拼接类型绘制），
    邻接掩码与具体变体在构建顶点时动态解析（resolve_cell_quads），
    因此相邻自动图块变化时无需改写
    网格数据。
*/

namespace jeecs::tilemap
{
    // ==================================================================
    // TilemapSystem：把地图文档实例化为分块渲染子实体
    // ==================================================================
    struct TilemapSystem : public game_system
    {
        // 分片标识：一个分片承载某图层某分块中来自某图集某源纹理的
        // 全部格子（不同源纹理顶点无法合批进同一缓冲）
        struct part_key
        {
            int32_t layer, chunk_x, chunk_y, tileset, source;
            bool operator<(const part_key& o) const;
        };
        // 图层样式（着色器实例与附加纹理）直接储存在文档的 LayerStyle 里：
        // 资源随文档加载就绪、uniform 值设在着色器实例上，渲染侧逐帧
        // 与文档比对即可，无需渲染侧再维护样式状态。

        // 每个地图根实体的跨帧簿记 + 上次网格扫描结果（版本戳命中时免扫）。
        // 注意：此处不允许缓存任何 je_game_entity_t——碎片整理与组件增删都
        // 会移动实体，隔帧句柄一律失效；分片/墙体实体每帧由 RenderPart/
        // WallPart 查询重新定位，跨帧只存纯数据（版本戳、掩码哈希等）
        struct root_state
        {
            // 分片表：part_key -> 上次顶点重建对应的版本戳（实体句柄
            // 不落簿记，当帧由查询结果配对）
            std::map<part_key, uint64_t> parts;
            std::set<part_key> needed;
            uint64_t needed_stamp;
            bool needed_valid;
            // 碰撞体墙簿记（无实体句柄，墙体当帧由 WallPart 查询定位）：
            // 掩码哈希用于把"版本戳变化但通行性未变"的编辑（纯视觉/
            // 样式）与真正的墙体重建区分开
            uint64_t wall_stamp;   // 上次掩码扫描对应的版本戳
            uint64_t wall_mask;    // 当前碰撞体实体对应的掩码哈希
            bool wall_block_empty; // 当前实体按哪个 block_empty 开关构建
            bool wall_valid;   // wall_stamp / wall_mask 是否有效
            // 本根实体当前持有的地图文档（nullopt = 无）：observe 取得
            // 的 shared_ptr 不占外部引用计数，根实体存续期间以其保活；
            // 路径变更/失效时随赋值自动换手释放，根销毁随析构释放
            std::optional<std::shared_ptr<Tilemap::MapDocument>> held_map;

            root_state()
                : needed_stamp(0)
                , needed_valid(false)
                , wall_stamp(0)
                , wall_mask(0)
                , wall_block_empty(false)
                , wall_valid(false)
            {
            }
        };

        std::unordered_map<uint32_t, root_state> _m_roots;
        std::vector<float> _m_staging;
        std::vector<uint8_t> _m_wall_mask;   // 不可通行掩码（复用暂存）

        TilemapSystem(game_world w);

        // ---- 分片生命周期 ----

        game_entity create_part_entity(game_entity root, int32_t layer, float layer_z,
            int32_t layer_rend_queue, je_layer_self_shadow_t layer_selfshadow,
            float selfshadow_factor,
            const Tilemap::SourceTexture& src, const Tilemap::LayerStyle& style);

        void rebuild_part(Tilemap::MapDocument& doc, game_entity root,
            const part_key& key, game_entity e);

        // ---- 碰撞体墙 ----

        game_entity create_wall_entity(game_entity root, const Tilemap::Wall& wall,
            const Transform::LocalScale* root_scale,
            int32_t x0, int32_t y0, int32_t x1, int32_t y1);

        // 掩码变化时整体重建：先回收全部旧碰撞体再按合并矩形新建。
        // 墙体属静态数据、编辑频度低，整体重建远比逐矩形差分简单可靠。
        // 严格模式的掩码向外扩了一圈，掩码坐标需平移回地图格坐标。
        // 旧墙体实体不落簿记：以当帧 WallPart 查询结果（stale_walls）
        // 回收；新建实体也只在本帧内使用
        void rebuild_walls(game_entity root,
            const Tilemap::Wall& wall,
            const std::vector<je_game_entity_t>& stale_walls,
            const std::vector<uint8_t>& blocked,
            int32_t mask_width, int32_t mask_height, bool block_empty);

        // 逐根同步碰撞体墙：Wall 组件离位/文档失效即回收；版本戳变化时
        // 重扫掩码，掩码哈希变化才重建实体；block_empty 开关不计入地图
        // 版本戳，组件侧开关翻转时强制重扫重建；每帧对齐 layerid 与比例尺。
        // live_walls 为当帧 WallPart 查询定位到的本根墙体实体（句柄当帧有效）
        void sync_root_walls(game_entity root, root_state& state,
            Tilemap::Wall* wall_comp, Tilemap::MapDocument* doc, uint64_t stamp,
            const std::vector<je_game_entity_t>& live_walls);

        void StateUpdate();
    };
}
