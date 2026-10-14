#pragma once

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#   error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"
#include "jeecs_core_tilemap_model.hpp"

#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <tuple>
#include <string>
#include <memory>
#include <cstring>
#include <algorithm>
#include <cassert>

/*
TilemapSystem [系统] 与 je_tilemap_* C API 实现
瓦片地图核心的渲染侧：把地图文档实例化为"图层 × 分块 × 图集源"的
渲染子实体。文档模型、序列化与自动图块解析见
jeecs_core_tilemap_model.hpp；本文件只做三件事：

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
  * 碰撞体墙（WallPart）同为根实体的子实体：根实体持有 Tilemap::Wall
    时，把不可通行格（任一图层不可通行即墙，与 je_tilemap_is_walkable
    同一语义）贪心合并为矩形，每矩形一个静态 Physics2D::Collider::Box，
    Rigidbody::layerid 取自 Wall::layerid。掩码哈希未变化的版本戳
    （纯视觉/样式编辑）不重建碰撞体，避免物理世界无谓抖动。
    Wall::block_empty 开启严格模式：无图块的空格与地图外缘（四周向外
    一圈）一并并入墙体，只有可通行图块覆盖的格子可通过。
  * 自动图块在网格中只存地形 id，邻接掩码与具体变体在构建顶点时
    动态解析（resolve_cell_quads），因此相邻地形变化时无需改写
    网格数据。
*/

namespace jeecs
{
    using namespace Renderer;
    using namespace Transform;

    // ==================================================================
    // TilemapSystem：把地图文档实例化为分块渲染子实体
    // ==================================================================
    struct TilemapSystem : public game_system
    {
        static constexpr size_t CHUNK = 32;         // 分块边长（格）
        static constexpr size_t FLOATS_PER_VERTEX = 9;   // pos3 + uv3(u,v,layer) + normal3
        static constexpr size_t VERTS_PER_CELL = 16;     // 每格 4 个子四边形槽位
        static constexpr size_t FLOATS_PER_CELL = VERTS_PER_CELL * FLOATS_PER_VERTEX;

        // 分片标识：一个分片承载某图层某分块中来自某图集某源纹理的
        // 全部格子（不同源纹理顶点无法合批进同一缓冲）
        struct part_key
        {
            int32_t layer = 0, chunk_x = 0, chunk_y = 0, tileset = 0, source = 0;
            bool operator<(const part_key& o) const
            {
                return std::tie(layer, chunk_x, chunk_y, tileset, source)
                    < std::tie(o.layer, o.chunk_x, o.chunk_y, o.tileset, o.source);
            }
        };
        struct part_entry
        {
            je_GameEntity entity{};
            uint64_t built_stamp = 0;
        };
        // 图层样式（着色器实例与附加纹理）直接储存在文档的 LayerStyle 里：
        // 资源随文档加载就绪、uniform 值设在着色器实例上，渲染侧逐帧
        // 与文档比对即可，无需渲染侧再维护样式状态。

        // 每个地图根实体的分片表 + 上次网格扫描结果（版本戳命中时免扫）
        struct root_state
        {
            std::map<part_key, part_entry> parts;
            std::set<part_key> needed;
            uint64_t needed_stamp = 0;
            bool needed_valid = false;
            // 碰撞体墙簿记：Wall 组件在位时按不可通行掩码合并出的
            // 静态 Box 碰撞体实体。掩码哈希用于把"版本戳变化但通行性
            // 未变"的编辑（纯视觉/样式）与真正的墙体重建区分开
            std::vector<je_GameEntity> walls;
            uint64_t wall_stamp = 0;   // 上次掩码扫描对应的版本戳
            uint64_t wall_mask = 0;    // 当前碰撞体实体对应的掩码哈希
            bool wall_block_empty = false; // 当前实体按哪个 block_empty 开关构建
            bool wall_valid = false;   // wall_stamp / wall_mask 是否有效
            // 本根实体当前持有的地图文档（nullopt = 无）：observe 取得
            // 的 shared_ptr 不占外部引用计数，根实体存续期间以其保活；
            // 路径变更/失效时随赋值自动换手释放，根销毁随析构释放
            std::optional<std::shared_ptr<Tilemap::MapDocument>> held_map;
        };

        std::unordered_map<uint32_t, root_state> _m_roots;
        std::vector<float> _m_staging;
        std::vector<uint8_t> _m_wall_mask;   // 不可通行掩码（复用暂存）

        TilemapSystem(game_world w)
            : game_system(w)
        {
            _m_staging.assign(CHUNK * CHUNK * FLOATS_PER_CELL, 0.f);
        }

        // ---- 共享资源 ----

        // 分片着色器与期望一致则不动，否则重写（资源句柄不同即更新）。
        // 着色器唯一来源是图层样式：未配置样式的层清空分片着色器，
        // 不回落地图根实体或内置默认（编辑器建图/建层时逐层指配）
        static void refresh_part_shader(const Tilemap::LayerStyle* style,
            const game_entity& part)
        {
            auto* comp = part.get_component<Renderer::Shaders>();
            if (comp == nullptr)
                return;
            if (style == nullptr || !style->shader.has_value())
            {
                comp->shaders.clear();
                return;
            }
            const auto& want = style->shader.value();
            if (comp->shaders.size() == 1
                && comp->shaders.front().get() == want.get())
                return;
            comp->shaders.clear();
            comp->shaders.push_back(want);
        }

        // 分片附加纹理对齐：Textures2D 只承载图层样式的 2D 附加纹理
        //（槽 >= 1，槽 0 由 Textures2DArray 的数组图集占用）；逐帧与
        // 样式比对重绑。与着色器/比例尺对齐同理——组件变化不计入地图
        // 版本戳，故每帧检查。
        static void refresh_part_textures(const game_entity& part,
            const Tilemap::LayerStyle* style)
        {
            auto* comp = part.get_component<Renderer::Textures2D>();
            if (comp == nullptr)
                return;

            const size_t want_count = style != nullptr ? style->textures.size() : 0;

            // 现状绑定与期望逐项比对，一致则不动
            size_t matched = 0;
            for (const auto& e : comp->textures)
            {
                if (matched >= want_count
                    || e.m_pass_id != (size_t)style->textures[matched].slot
                    || e.m_texture.get() != style->textures[matched].texture.get())
                {
                    matched = want_count + 1;    // 标记不一致
                    break;
                }
                ++matched;
            }
            if (matched == want_count)
                return;

            // 重绑：按样式重建
            comp->textures.clear();
            if (style != nullptr)
                for (const auto& t : style->textures)
                    comp->bind_texture((size_t)t.slot, t.texture);
        }

        // 比例尺同步：把根实体 LocalScale 复制到分片 LocalScale。
        // 变换系统的缩放为纯局部语义（不沿父链复合，设计如此），
        // 瓦片地图的比例尺因此由本系统显式维护。
        static void sync_part_scale(const game_entity& root, const game_entity& part)
        {
            auto* root_scale = root.get_component<Transform::LocalScale>();
            auto* part_scale = part.get_component<Transform::LocalScale>();
            if (root_scale == nullptr || part_scale == nullptr)
                return;
            if (part_scale->scale.x != root_scale->scale.x
                || part_scale->scale.y != root_scale->scale.y
                || part_scale->scale.z != root_scale->scale.z)
            {
                part_scale->scale = root_scale->scale;
            }
        }

        // 分块静态角点索引：每格 4 个四边形槽位，各 6 索引（0,1,2 2,1,3）
        static const std::vector<uint32_t>& shared_indices()
        {
            static std::vector<uint32_t> indices = []
                {
                    std::vector<uint32_t> idx;
                    idx.reserve(CHUNK * CHUNK * 4 * 6);
                    for (size_t c = 0; c < CHUNK * CHUNK; ++c)
                        for (size_t q = 0; q < 4; ++q)
                        {
                            const uint32_t base = (uint32_t)(c * VERTS_PER_CELL + q * 4);
                            for (uint32_t i : { base + 0, base + 1, base + 2,
                                base + 2, base + 1, base + 3 })
                                idx.push_back(i);
                        }
                    return idx;
                }();
            return indices;
        }

        // ---- 顶点写入 ----

        // 写一个四边形（x0/y0 左下，x1/y1 右上；v 引擎空间向上增长；
        // uv 为 tile-local [0,1]²，layer 为数组纹理层号）
        static void write_quad(float* v,
            float x0, float y0, float x1, float y1,
            float u0, float v0, float u1, float v1, float layer)
        {
            // 顶点序：0=左下 1=右下 2=左上 3=右上（索引 0,1,2 2,1,3）
            float* p = v;
            *p++ = x0; *p++ = y0; *p++ = 0.f; *p++ = u0; *p++ = v0; *p++ = layer; *p++ = 0.f; *p++ = 0.f; *p++ = -1.f;
            *p++ = x1; *p++ = y0; *p++ = 0.f; *p++ = u1; *p++ = v0; *p++ = layer; *p++ = 0.f; *p++ = 0.f; *p++ = -1.f;
            *p++ = x0; *p++ = y1; *p++ = 0.f; *p++ = u0; *p++ = v1; *p++ = layer; *p++ = 0.f; *p++ = 0.f; *p++ = -1.f;
            *p++ = x1; *p++ = y1; *p++ = 0.f; *p++ = u1; *p++ = v1; *p++ = layer; *p++ = 0.f; *p++ = 0.f; *p++ = -1.f;
        }

        // 把某格的解析结果写入 staging（16 顶点槽位：未用槽位写零面积
        // 退化四边形，不产生像素）。几何约定：1 格 = 1 世界单位
        //（tile_px 只用于图集采样 UV；根实体 LocalScale 即比例尺）。
        static void write_cell(float* dst, int32_t x, int32_t y,
            const Tilemap::cell_quad_info* quads, int32_t count)
        {
            const float x0 = (float)x, x1 = x0 + 1.f;
            const float ytop = -(float)y, ybot = ytop - 1.f;
            const float half = 0.5f;
            const float xm = x0 + half, ym = ybot + half;

            for (size_t q = 0; q < 4; ++q)
                write_quad(dst + q * 4 * FLOATS_PER_VERTEX, x0, ytop, x0, ytop, 0.f, 0.f, 0.f, 0.f, 0.f);
            for (int32_t i = 0; i < count; ++i)
            {
                const auto& q = quads[i];
                if (!q.corner.has_value())
                {
                    write_quad(dst, x0, ybot, x1, ytop, q.u0, q.v0, q.u1, q.v1, (float)q.layer);
                }
                else
                {
                    // 四象限槽位：0=LU 1=RU 2=LD 3=RD
                    float cx0, cy0, cx1, cy1;
                    switch (*q.corner)
                    {
                    case 0: cx0 = x0; cy0 = ym;  cx1 = xm; cy1 = ytop; break;
                    case 1: cx0 = xm;  cy0 = ym;  cx1 = x1; cy1 = ytop; break;
                    case 2: cx0 = x0;  cy0 = ybot; cx1 = xm; cy1 = ym;  break;
                    default: cx0 = xm;  cy0 = ybot; cx1 = x1; cy1 = ym;  break;
                    }
                    write_quad(dst + (size_t)*q.corner * 4 * FLOATS_PER_VERTEX,
                        cx0, cy0, cx1, cy1, q.u0, q.v0, q.u1, q.v1, (float)q.layer);
                }
            }
        }

        // ---- 分片生命周期 ----

        game_entity create_part_entity(game_entity root, int32_t layer, float layer_z,
            int32_t layer_rend_queue,
            const Tilemap::SourceTexture& src, const Tilemap::LayerStyle& style)
        {
            auto world = get_world();
            auto part = world.add_entity<
                Transform::LocalPosition, Transform::LocalRotation, Transform::LocalScale,
                Transform::LocalToParent, Transform::Translation,
                Renderer::Shape, Renderer::Shaders, Renderer::Textures2D,
                Renderer::Textures2DArray,
                Renderer::Rendqueue, Editor::Invisible, Tilemap::RenderPart>();

            // 挂到地图根实体下（根实体需要 Anchor）
            auto* anchor = root.get_component<Transform::Anchor>();
            if (anchor == nullptr)
                anchor = root.add_component<Transform::Anchor>();
            auto* l2p = part.get_component<Transform::LocalToParent>();
            l2p->parent_uid = anchor->uid;

            auto* pos = part.get_component<Transform::LocalPosition>();
            // 图层 z 由地图数据（图层属性）指定，可手动调整
            pos->pos = math::vec3(0.f, 0.f, layer_z);
            // 渲染队列同取自图层属性：同图各层按 rend_queue 升序绘制
            part.get_component<Renderer::Rendqueue>()->rend_queue = layer_rend_queue;

            // 比例尺：根实体的缩放即整图比例尺（变换系统的缩放为纯局部
            // 语义、不沿父链复合，故由本系统显式同步到各分片）
            if (auto* root_scale = root.get_component<Transform::LocalScale>())
                part.get_component<Transform::LocalScale>()->scale = root_scale->scale;

            // 着色器：唯一来源是图层样式（编辑器建图/建层时逐层指配；
            // 未配置样式的层分片着色器列表为空，渲染端按通用规则处理）
            auto* shaders = part.get_component<Renderer::Shaders>();
            if (style.shader.has_value())
                shaders->shaders.push_back(style.shader.value());

            // 数组图集（每层一个瓦片）绑到 pass 0：采样侧 tex2darray，
            // 相邻瓦片渗色在结构上不可能发生（层内 CLAMP）
            if (src.array.has_value())
                part.get_component<Renderer::Textures2DArray>()
                    ->bind_texture(0, *src.array);
            else
                debug::logerr(
                    "Tilemap: source '%s' has no texture array built yet.",
                    src.path.c_str());

            // 图层样式的附加 2D 纹理占其余槽（槽 >= 1）
            auto* textures = part.get_component<Renderer::Textures2D>();
            for (const auto& t : style.textures)
                textures->bind_texture((size_t)t.slot, t.texture);

            auto* shape = part.get_component<Renderer::Shape>();
            auto created = graphic::vertex::create(
                jegl_vertex::TRIANGLES,
                _m_staging.data(),
                _m_staging.size() * sizeof(float),
                shared_indices(),
                {
                    { jegl_vertex::data_type::FLOAT32, 3 }, // pos
                    { jegl_vertex::data_type::FLOAT32, 3 }, // uv (u, v, array layer)
                    { jegl_vertex::data_type::FLOAT32, 3 }, // normal
                });
            if (created.has_value())
                shape->vertex = created;

            return part;
        }

        void rebuild_part(Tilemap::MapDocument& doc, game_entity root,
            const part_key& key, part_entry& part)
        {
            game_entity e{ part.entity };
            auto* shape = e.get_component<Renderer::Shape>();
            if (shape == nullptr || !shape->vertex.has_value())
                return;

            memset(_m_staging.data(), 0, _m_staging.size() * sizeof(float));
            size_t used_cells = 0;
            const int32_t bx = key.chunk_x * (int32_t)CHUNK;
            const int32_t by = key.chunk_y * (int32_t)CHUNK;

            for (int32_t ly = 0; ly < (int32_t)CHUNK; ++ly)
            {
                int32_t y = by + ly;
                if (y >= doc.height) break;
                for (int32_t lx = 0; lx < (int32_t)CHUNK; ++lx)
                {
                    int32_t x = bx + lx;
                    if (x >= doc.width) break;
                    Tilemap::cell_quad_info quads[4];
                    int32_t count = Tilemap::resolve_cell_quads(doc, key.layer, x, y, quads);
                    if (count <= 0)
                        continue;
                    // 该格须属于此分片的 (图集, 源纹理)
                    if (quads[0].tileset_idx != key.tileset
                        || quads[0].source_idx != key.source)
                        continue;
                    write_cell(_m_staging.data() + used_cells * FLOATS_PER_CELL,
                        x, y, quads, count);
                    ++used_cells;
                }
            }

            jegl_vertex* raw = shape->vertex->get()->resource();
            shape->vertex->get()->update_buffer(
                _m_staging.data(),
                _m_staging.size() * sizeof(float),
                used_cells * 4 * 6);

            // 分块包围盒（局部空间：1 格 = 1 单位，向 +x 与 -y 展开）
            raw->m_x_min = (float)bx;
            raw->m_x_max = (float)std::min<int32_t>(bx + (int32_t)CHUNK, doc.width);
            raw->m_y_max = -(float)by;
            raw->m_y_min = -(float)std::min<int32_t>(by + (int32_t)CHUNK, doc.height);
            const float lz = doc.layers[key.layer].z;
            raw->m_z_min = lz - 0.01f;
            raw->m_z_max = lz + 0.01f;

            // 层 z / rend_queue 变化时同步分片；比例尺兜底写入
            //（常态由 sync_part_scale 每帧对齐，此处确保任何路径都不漏）
            if (auto* pos = e.get_component<Transform::LocalPosition>())
                pos->pos = math::vec3(0.f, 0.f, lz);
            if (auto* rendqueue = e.get_component<Renderer::Rendqueue>())
                rendqueue->rend_queue = doc.layers[key.layer].rend_queue;
            if (auto* root_scale = root.get_component<Transform::LocalScale>())
            {
                if (auto* sc = e.get_component<Transform::LocalScale>())
                    sc->scale = root_scale->scale;
            }
        }

        // ---- 网格扫描：收集某文档当前需要的全部分片 ----
        static void scan_needed(Tilemap::MapDocument& doc, std::set<part_key>& needed)
        {
            auto& docs = Tilemap::documents::inst();
            docs.ensure_tilesets(doc);
            for (size_t li = 0; li < doc.layers.size(); ++li)
            {
                if (!doc.layers[li].visible)
                    continue;
                const auto& grid = doc.layers[li].grid;
                for (int32_t y = 0; y < doc.height; ++y)
                {
                    for (int32_t x = 0; x < doc.width; ++x)
                    {
                        int32_t v = grid[y * doc.width + x];
                        if (v == 0) continue;
                        auto cell = Tilemap::decode_tile_value(v);
                        if (!cell.has_value()
                            || cell->tileset_idx < 0
                            || cell->tileset_idx >= (int32_t)doc.tilesets.size()
                            || !doc.tilesets[cell->tileset_idx].resolved.has_value())
                            continue;
                        const auto* ts = doc.tilesets[cell->tileset_idx].resolved->get();
                        std::optional<int32_t> src_idx;
                        if (!cell->is_terrain)
                        {
                            if (cell->local_id > 0 && cell->local_id < (int32_t)ts->tiles.size())
                                src_idx = ts->tiles[cell->local_id].source;
                        }
                        else if (cell->local_id > 0 && cell->local_id < (int32_t)ts->terrains.size())
                            src_idx = ts->terrains[cell->local_id].source;
                        if (!src_idx.has_value() || *src_idx >= (int32_t)ts->sources.size())
                            continue;
                        needed.insert(part_key{
                            (int32_t)li, x / (int32_t)CHUNK, y / (int32_t)CHUNK,
                            cell->tileset_idx, *src_idx });
                    }
                }
            }
        }

        // ---- 碰撞体墙：不可通行掩码 → 静态 Box 碰撞体子实体 ----

        // 扫描不可通行掩码，返回掩码哈希（FNV-1a，混入 block_empty 开关位）：
        // 版本戳变化但通行性未变时免重建碰撞体。out_width/out_height 回报
        // 掩码尺寸（严格模式含外扩圈）。
        // 第一轮与 je_tilemap_is_walkable 同一语义：任一图层不可通行即墙，
        // 空格/无法解析的格子视为可通行（逐格内联 walkable_on 的判定，
        // 避免每格经注册表锁解析图集）。
        // block_empty 严格模式：掩码全 1 起步并向四周外扩一圈（地图外缘
        // 恒为墙），第一轮以标记 2 区分"判定墙"与初始填充，第二轮把
        // "未被判定且有图块（任一图层非空）"的格子清 0 —— 完全没有图块
        // 的空格保持墙，外扩圈与贴边墙体贪心合并成整块。
        static uint64_t scan_wall_mask(Tilemap::MapDocument& doc,
            std::vector<uint8_t>& blocked, bool block_empty,
            int32_t& out_width, int32_t& out_height)
        {
            Tilemap::documents::inst().ensure_tilesets(doc);
            const int32_t mw = doc.width + (block_empty ? 2 : 0);
            const int32_t mh = doc.height + (block_empty ? 2 : 0);
            out_width = mw;
            out_height = mh;
            blocked.assign((size_t)mw * mh, block_empty ? 1 : 0);
            for (const auto& layer : doc.layers)
            {
                const auto& grid = layer.grid;
                for (int32_t y = 0; y < doc.height; ++y)
                {
                    for (int32_t x = 0; x < doc.width; ++x)
                    {
                        const size_t di =
                            (size_t)(y + (block_empty ? 1 : 0)) * mw
                            + (size_t)(x + (block_empty ? 1 : 0));
                        if (!block_empty && blocked[di])
                            continue;   // 已判定为墙，其他层无需再看
                        const int32_t v = grid[(size_t)y * doc.width + x];
                        if (v == 0)
                            continue;   // 空格：普通模式可通行；严格模式留待第二轮
                        auto cell = Tilemap::decode_tile_value(v);
                        if (!cell.has_value()
                            || cell->tileset_idx < 0
                            || cell->tileset_idx >= (int32_t)doc.tilesets.size()
                            || !doc.tilesets[cell->tileset_idx].resolved.has_value())
                            continue;
                        const auto* ts = doc.tilesets[cell->tileset_idx].resolved->get();
                        const int32_t local = cell->local_id;
                        const bool walkable = cell->is_terrain
                            ? (local >= (int32_t)ts->terrains.size()
                                || ts->terrains[local].walkable)
                            : (local >= (int32_t)ts->tiles.size()
                                || ts->tiles[local].walkable);
                        if (!walkable)
                            blocked[di] = block_empty ? 2 : 1;
                    }
                }
            }
            if (block_empty)
            {
                for (int32_t y = 0; y < doc.height; ++y)
                {
                    for (int32_t x = 0; x < doc.width; ++x)
                    {
                        const size_t di = (size_t)(y + 1) * mw + (size_t)(x + 1);
                        if (blocked[di] == 2)
                        {
                            blocked[di] = 1;     // 任一图层不可通行：判定墙
                            continue;
                        }
                        // 初始填充（=1）：有图块才可通过，空格保持墙
                        bool has_tile = false;
                        for (const auto& layer : doc.layers)
                            if (layer.grid[(size_t)y * doc.width + x] != 0)
                            {
                                has_tile = true;
                                break;
                            }
                        if (has_tile)
                            blocked[di] = 0;
                    }
                }
            }
            uint64_t hash = 0x9E3779B97F4A7C15ull;
            hash = (hash ^ (uint64_t)(block_empty ? 1 : 0)) * 0x100000001B3ull;
            for (uint8_t v : blocked)
                hash = (hash ^ v) * 0x100000001B3ull;
            return hash;
        }

        // 掩码贪心合并为互不重叠的最大矩形（x0/y0/x1/y1 为含端点的格坐标）：
        // 从每个未占用的墙格向右取最长行段，再向下逐行整段扩展
        static void merge_wall_rects(const std::vector<uint8_t>& blocked,
            int32_t width, int32_t height, std::vector<int32_t>& rects)
        {
            std::vector<uint8_t> used((size_t)width * height, 0);
            for (int32_t y = 0; y < height; ++y)
            {
                for (int32_t x = 0; x < width; ++x)
                {
                    const size_t i = (size_t)y * width + x;
                    if (!blocked[i] || used[i])
                        continue;
                    int32_t x1 = x;
                    while (x1 + 1 < width
                        && blocked[i + (size_t)(x1 + 1 - x)]
                        && !used[i + (size_t)(x1 + 1 - x)])
                        ++x1;
                    int32_t y1 = y;
                    while (y1 + 1 < height)
                    {
                        bool ok = true;
                        for (int32_t xx = x; xx <= x1; ++xx)
                        {
                            const size_t j = (size_t)(y1 + 1) * width + xx;
                            if (!blocked[j] || used[j])
                            {
                                ok = false;
                                break;
                            }
                        }
                        if (!ok)
                            break;
                        ++y1;
                    }
                    for (int32_t yy = y; yy <= y1; ++yy)
                        for (int32_t xx = x; xx <= x1; ++xx)
                            used[(size_t)yy * width + xx] = 1;
                    rects.insert(rects.end(), { x, y, x1, y1 });
                }
            }
        }

        // 碰撞体随动：layerid 跟随 Wall 组件；地图局部几何按根实体比例尺
        // 预缩放——变换系统的缩放为纯局部语义（不沿父链复合，见
        // sync_part_scale），物理位姿又只取 Translation 的世界平移/旋转，
        // 故把缩放折进局部位置与 Box 尺寸，与渲染分片的视觉比例对齐
        static void sync_wall_entity(const game_entity& e,
            const Tilemap::Wall& wall, const Transform::LocalScale* root_scale)
        {
            const float sx = root_scale != nullptr ? std::abs(root_scale->scale.x) : 1.f;
            const float sy = root_scale != nullptr ? std::abs(root_scale->scale.y) : 1.f;
            if (auto* rb = e.get_component<Physics2D::Rigidbody>())
                rb->layerid = wall.layerid;
            auto* tag = e.get_component<Tilemap::WallPart>();
            if (tag == nullptr)
                return;
            if (auto* pos = e.get_component<Transform::LocalPosition>())
            {
                const math::vec3 want(tag->center_x * sx, tag->center_y * sy, 0.f);
                if (pos->pos.x != want.x || pos->pos.y != want.y)
                    pos->pos = want;
            }
            if (auto* box = e.get_component<Physics2D::Collider::Box>())
            {
                const math::vec2 want(tag->size_x * sx, tag->size_y * sy);
                if (box->size.x != want.x || box->size.y != want.y)
                    box->size = want;
            }
        }

        game_entity create_wall_entity(game_entity root, const Tilemap::Wall& wall,
            const Transform::LocalScale* root_scale,
            int32_t x0, int32_t y0, int32_t x1, int32_t y1)
        {
            auto world = get_world();
            // 静态碰撞体：无 DynamicBody/KinematicBody 即 Static（墙不可推动）
            auto e = world.add_entity<
                Transform::LocalPosition, Transform::LocalRotation, Transform::LocalScale,
                Transform::LocalToParent, Transform::Translation,
                Physics2D::Rigidbody, Physics2D::Collider::Box,
                Editor::Invisible, Tilemap::WallPart>();

            // 挂到地图根实体下，随根移动/旋转（根实体需要 Anchor）
            auto* anchor = root.get_component<Transform::Anchor>();
            if (anchor == nullptr)
                anchor = root.add_component<Transform::Anchor>();
            e.get_component<Transform::LocalToParent>()->parent_uid = anchor->uid;

            // 地图局部几何：1 格 = 1 单位、向 +x 与 -y 展开（write_cell
            // 同款约定），格 (x,y) 占据 y ∈ [-(y+1), -y]
            auto* tag = e.get_component<Tilemap::WallPart>();
            tag->owner_id = root._m_raw._m_id;
            tag->center_x = 0.5f * (float)(x0 + x1 + 1);
            tag->center_y = -0.5f * (float)(y0 + y1 + 1);
            tag->size_x = (float)(x1 - x0 + 1);
            tag->size_y = (float)(y1 - y0 + 1);

            sync_wall_entity(e, wall, root_scale);
            return e;
        }

        // 掩码变化时整体重建：先回收全部旧碰撞体再按合并矩形新建。
        // 墙体属静态数据、编辑频度低，整体重建远比逐矩形差分简单可靠。
        // 严格模式的掩码向外扩了一圈，掩码坐标需平移回地图格坐标
        void rebuild_walls(game_entity root, root_state& state,
            const Tilemap::Wall& wall, const std::vector<uint8_t>& blocked,
            int32_t mask_width, int32_t mask_height, bool block_empty)
        {
            auto world = get_world();
            for (auto& raw : state.walls)
                world.remove_entity(game_entity{ raw });
            state.walls.clear();

            const auto* root_scale = root.get_component<Transform::LocalScale>();
            std::vector<int32_t> rects;
            merge_wall_rects(blocked, mask_width, mask_height, rects);
            const int32_t off = block_empty ? 1 : 0;
            state.walls.reserve(rects.size() / 4);
            for (size_t r = 0; r + 3 < rects.size(); r += 4)
                state.walls.push_back(create_wall_entity(
                    root, wall, root_scale,
                    rects[r] - off, rects[r + 1] - off,
                    rects[r + 2] - off, rects[r + 3] - off)._m_raw);
        }

        // 逐根同步碰撞体墙：Wall 组件离位/文档失效即回收；版本戳变化时
        // 重扫掩码，掩码哈希变化才重建实体；block_empty 开关不计入地图
        // 版本戳，组件侧开关翻转时强制重扫重建；每帧对齐 layerid 与比例尺
        void sync_root_walls(game_entity root, root_state& state,
            Tilemap::Wall* wall_comp, Tilemap::MapDocument* doc, uint64_t stamp)
        {
            if (wall_comp == nullptr || doc == nullptr)
            {
                if (!state.walls.empty())
                {
                    auto world = get_world();
                    for (auto& raw : state.walls)
                        world.remove_entity(game_entity{ raw });
                    state.walls.clear();
                }
                state.wall_valid = false;
                return;
            }
            const bool settings_changed = !state.wall_valid
                || state.wall_block_empty != wall_comp->block_empty;
            if (settings_changed || state.wall_stamp != stamp)
            {
                int32_t mw = 0, mh = 0;
                const uint64_t mask_hash = scan_wall_mask(
                    *doc, _m_wall_mask, wall_comp->block_empty, mw, mh);
                if (settings_changed || mask_hash != state.wall_mask)
                {
                    rebuild_walls(root, state, *wall_comp,
                        _m_wall_mask, mw, mh, wall_comp->block_empty);
                    state.wall_mask = mask_hash;
                    state.wall_block_empty = wall_comp->block_empty;
                }
                state.wall_stamp = stamp;
                state.wall_valid = true;
            }
            const auto* root_scale = root.get_component<Transform::LocalScale>();
            for (auto& raw : state.walls)
                sync_wall_entity(game_entity{ raw }, *wall_comp, root_scale);
        }

        void Update()
        {
            auto world = get_world();
            auto& docs = Tilemap::documents::inst();

            // 1. 收集地图根实体并解析文档（Wall 为可选的碰撞体开关）
            struct root_info
            {
                je_GameEntity raw{};
                Tilemap::Map* comp = nullptr;
                Tilemap::Wall* wall = nullptr;
                std::optional<std::shared_ptr<Tilemap::MapDocument>> doc;
            };
            std::vector<root_info> roots;
            std::set<uint32_t> alive_root_ids;
            for (auto&& [e, map_comp, wall_comp] :
                query_entity<view typesof(Tilemap::Map&, Tilemap::Wall*)>())
            {
                alive_root_ids.insert(e._m_raw._m_id);
                roots.push_back({ e._m_raw, &map_comp, wall_comp, std::nullopt });
            }
            for (auto& r : roots)
            {
                const std::string path =
                    r.comp != nullptr ? std::string(r.comp->map_path.c_str()) : std::string();
                r.doc = docs.observe_map(path);
            }

            // 2. 回收孤儿分片与根簿记（所有者不存在或不再持有 Map 组件）
            std::vector<je_GameEntity> dead_parts;
            for (auto&& [e, part] :
                query_entity<view typesof(Tilemap::RenderPart&)>())
            {
                if (alive_root_ids.count(part.owner_id) == 0)
                    dead_parts.push_back(e._m_raw);
            }
            for (auto&& [e, part] :
                query_entity<view typesof(Tilemap::WallPart&)>())
            {
                if (alive_root_ids.count(part.owner_id) == 0)
                    dead_parts.push_back(e._m_raw);
            }
            for (auto& raw : dead_parts)
                world.remove_entity(game_entity{ raw });
            for (auto it = _m_roots.begin(); it != _m_roots.end();)
            {
                if (alive_root_ids.count(it->first) == 0)
                {
                    // 根实体消失：地图持有随 root_state 析构自动释放
                    it = _m_roots.erase(it);
                }
                else
                    ++it;
            }

            // 3. 逐根同步分片集合与版本
            for (auto& r : roots)
            {
                uint32_t root_id = r.raw._m_id;
                root_state& state = _m_roots[root_id];
                game_entity root{ r.raw };

                // 世界占用：根实体存续期间以 observe 持有地图实例（不占
                // 外部引用计数）；路径变更（另存/换绑）或首次解析时随
                // 赋值自动换手，同实例赋值为无操作
                state.held_map = r.doc;
                Tilemap::MapDocument* const doc =
                    r.doc.has_value() ? r.doc->get() : nullptr;

                // 网格扫描只在版本戳变化时执行（map_stamp 已含图集实例身份，
                // 图集被重载/替换同样会触发重扫）
                uint64_t stamp = 0;
                if (doc != nullptr)
                    stamp = docs.map_stamp(*doc);
                if (!state.needed_valid || state.needed_stamp != stamp)
                {
                    // 图集数组兜底：文档解析与 add_source 正常都会构建，
                    // 此处对经由其他路径装载的文档在版本变化时幂等补建
                    //（纹理未就绪时由分片创建分支再兜底）
                    if (doc != nullptr)
                        for (auto& ref : doc->tilesets)
                            if (ref.resolved.has_value())
                                for (auto& s : ref.resolved->get()->sources)
                                    if (!s.array.has_value())
                                        Tilemap::build_source_array(s);

                    state.needed.clear();
                    if (doc != nullptr)
                        scan_needed(*doc, state.needed);
                    state.needed_stamp = stamp;
                    state.needed_valid = true;
                }
                const std::set<part_key>& needed = state.needed;

                // 删除不再需要的分片
                std::vector<part_key> removed;
                for (auto& [key, entry] : state.parts)
                    if (needed.count(key) == 0)
                        removed.push_back(key);
                for (const auto& key : removed)
                {
                    world.remove_entity(game_entity{ state.parts.at(key).entity });
                    state.parts.erase(key);
                }

                // 重建/刷新保留与新增的分片
                for (const auto& key : needed)
                {
                    // 图层样式直接取自文档（资源随文档加载就绪）；文档
                    // 不可用或层下标越界按无样式处理（清空分片着色器）
                    const Tilemap::LayerStyle* style =
                        doc != nullptr
                        && key.layer < (int32_t)doc->layers.size()
                        ? &doc->layers[key.layer].style : nullptr;

                    auto fnd = state.parts.find(key);
                    if (fnd != state.parts.end())
                    {
                        if (doc != nullptr && fnd->second.built_stamp != stamp)
                        {
                            rebuild_part(*doc, root, key, fnd->second);
                            fnd->second.built_stamp = stamp;
                        }
                        // 着色器/纹理跟随图层样式，比例尺跟随根实体（组件
                        // 与样式变化均不计入地图版本戳，故每帧对齐）
                        game_entity part{ fnd->second.entity };
                        refresh_part_shader(style, part);
                        refresh_part_textures(part, style);
                        sync_part_scale(root, part);
                        continue;
                    }

                    Tilemap::SourceTexture* src_info = nullptr;
                    if (doc != nullptr
                        && key.tileset < (int32_t)doc->tilesets.size()
                        && doc->tilesets[key.tileset].resolved.has_value()
                        && key.source < (int32_t)doc->tilesets[key.tileset].resolved->get()->sources.size())
                        src_info = &doc->tilesets[key.tileset].resolved->get()->sources[key.source];
                    if (src_info == nullptr)
                        continue;

                    // 数组图集就绪兜底（首次创建该分片时构建一次；
                    // 纹理未就绪则本次留空，版本变化时重扫再试）
                    if (!src_info->array.has_value())
                        Tilemap::build_source_array(*src_info);

                    game_entity part_entity = create_part_entity(
                        root, key.layer, doc->layers[key.layer].z,
                        doc->layers[key.layer].rend_queue, *src_info,
                        style != nullptr ? *style : Tilemap::LayerStyle{});
                    auto* tag = part_entity.get_component<Tilemap::RenderPart>();
                    tag->owner_id = root_id;
                    tag->layer = key.layer;
                    tag->chunk_x = key.chunk_x;
                    tag->chunk_y = key.chunk_y;
                    tag->source = key.tileset * 1024 + key.source;

                    part_entry entry{};
                    entry.entity = part_entity._m_raw;
                    if (doc != nullptr)
                    {
                        rebuild_part(*doc, root, key, entry);
                        entry.built_stamp = stamp;
                    }
                    state.parts[key] = entry;
                }

                // 4. 碰撞体墙同步：Wall 组件在位时按不可通行掩码
                //    生成/回收静态 Physics2D 碰撞体子实体
                sync_root_walls(root, state, r.wall, doc, stamp);
            }
        }
    };
}

// ======================================================================
// je_tilemap_* C API 实现（声明与 JE_API 导出标记统一见 include/jeecs.hpp）
// ======================================================================

// ---------------- 地图文档 ----------------
/* OPTIONAL: If path cannot be opened */ je_TilemapDocument* je_tilemap_open_map(const char* path)
{
    assert(path != nullptr);
    // open_map 返回自带 1 个外部引用的实例（unload/gchandle 回收时
    // 释放）；打开失败（文件缺失/损坏，含已标记失败不重试）为 nullptr
    auto doc = jeecs::Tilemap::documents::inst().open_map(path);
    return doc.has_value() ? doc->get() : nullptr;
}
/* OPTIONAL: If size is invalid */ je_TilemapDocument* je_tilemap_create_map(int32_t w, int32_t h, int32_t tile_px)
{
    if (w <= 0 || h <= 0 || tile_px <= 0 || w > 65536 || h > 65536)
        return nullptr;
    auto doc = std::make_shared<jeecs::Tilemap::MapDocument>();
    doc->width = w; doc->height = h; doc->tile_px = tile_px;
    jeecs::Tilemap::MapLayer layer;
    layer.name = "图层 1";
    layer.grid.assign((size_t)w * h, 0);
    doc->layers.push_back(std::move(layer));
    // register_map 返回自带 1 个引用的实例
    return jeecs::Tilemap::documents::inst().register_map(std::move(doc));
}
int32_t je_tilemap_keep_map(je_TilemapDocument* map)
{
    return jeecs::Tilemap::documents::inst().keep_map(map);
}
int32_t je_tilemap_unload_map(je_TilemapDocument* map)
{
    return jeecs::Tilemap::documents::inst().unload_map(map);
}
bool je_tilemap_save_map(je_TilemapDocument* map, const char* path)
{
    assert(path != nullptr);
    return jeecs::Tilemap::documents::inst().save_map(map, path);
}
void je_tilemap_reload_map(const char* path)
{
    assert(path != nullptr);
    jeecs::Tilemap::documents::inst().reload_map(path);
}
void je_tilemap_map_size(je_TilemapDocument* doc, /* OPTIONAL */ int32_t* w, /* OPTIONAL */ int32_t* h, /* OPTIONAL */ int32_t* tile_px, /* OPTIONAL */ int32_t* layer_count)
{
    if (w != nullptr) *w = doc->width;
    if (h != nullptr) *h = doc->height;
    if (tile_px != nullptr) *tile_px = doc->tile_px;
    if (layer_count != nullptr) *layer_count = (int32_t)doc->layers.size();
}
bool je_tilemap_map_path(je_TilemapDocument* doc, const char** out_path)
{
    assert(out_path != nullptr);
    *out_path = doc->path.c_str();
    return !doc->path.empty();
}
uint64_t je_tilemap_map_version(je_TilemapDocument* doc)
{
    return doc->version;
}
int32_t je_tilemap_loaded_map_count(void)
{
    return jeecs::Tilemap::documents::inst().map_count();
}
/* OPTIONAL: If index is out of range or the slot is empty */ je_TilemapDocument* je_tilemap_loaded_map(int32_t index)
{
    // 调用期间由 map_at 锁内的持有保活，用毕即弃、不占外部引用
    auto doc = jeecs::Tilemap::documents::inst().map_at(index);
    return doc.has_value() ? doc->get() : nullptr;
}
void je_tilemap_resize_map(je_TilemapDocument* doc, int32_t new_w, int32_t new_h)
{
    if (new_w <= 0 || new_h <= 0 || new_w > 65536 || new_h > 65536)
        return;
    for (auto& layer : doc->layers)
    {
        std::vector<int32_t> resized((size_t)new_w * new_h, 0);
        for (int32_t y = 0; y < std::min(new_h, doc->height); ++y)
            for (int32_t x = 0; x < std::min(new_w, doc->width); ++x)
                resized[y * new_w + x] = layer.grid[y * doc->width + x];
        layer.grid = std::move(resized);
    }
    // 清除越界格子的属性覆盖
    decltype(doc->cell_properties) kept;
    for (auto& [key, kvs] : doc->cell_properties)
    {
        int32_t x = key.second % doc->width, y = key.second / doc->width;
        if (x < new_w && y < new_h)
            kept[{ key.first, x + y * new_w }] = std::move(kvs);
    }
    doc->cell_properties = std::move(kept);
    doc->width = new_w;
    doc->height = new_h;
    ++doc->version;
}

// ---------------- 图层 ----------------
bool je_tilemap_add_layer(je_TilemapDocument* doc, const char* name, je_LayerId* out_id)
{
    assert(name != nullptr);
    assert(out_id != nullptr);
    jeecs::Tilemap::MapLayer layer;
    layer.name = name;
    layer.z = (float)doc->layers.size(); // 新层默认置于最上（沿 +z 错开）
    layer.rend_queue = jeecs::Tilemap::LAYER_REND_QUEUE_BASE
        + (int32_t)doc->layers.size();   // 渲染队列同样按下标错开（升序绘制）
    layer.grid.assign((size_t)doc->width * doc->height, 0);
    doc->layers.push_back(std::move(layer));
    ++doc->version;
    *out_id = (int32_t)doc->layers.size() - 1;
    return true;
}
bool je_tilemap_remove_layer(je_TilemapDocument* doc, je_LayerId layer)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size())
        return false;
    doc->layers.erase(doc->layers.begin() + layer);
    // 图层下标变化：重建逐格属性键
    decltype(doc->cell_properties) kept;
    for (auto& [key, kvs] : doc->cell_properties)
    {
        if (key.first == layer) continue;
        kept[{ key.first > layer ? key.first - 1 : key.first, key.second }] = std::move(kvs);
    }
    doc->cell_properties = std::move(kept);
    ++doc->version;
    return true;
}
bool je_tilemap_move_layer(je_TilemapDocument* doc, je_LayerId layer, je_LayerId new_pos)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size())
        return false;
    int32_t count = (int32_t)doc->layers.size();
    if (new_pos < 0) new_pos = 0;
    if (new_pos >= count) new_pos = count - 1;
    if (layer == new_pos)
        return true;
    auto moved = std::move(doc->layers[layer]);
    doc->layers.erase(doc->layers.begin() + layer);
    doc->layers.insert(doc->layers.begin() + new_pos, std::move(moved));
    // 图层下标变化：重排逐格属性键
    decltype(doc->cell_properties) kept;
    for (auto& [key, kvs] : doc->cell_properties)
    {
        int32_t l = key.first;
        if (l == layer) l = new_pos;
        else if (layer < new_pos && l > layer && l <= new_pos) l -= 1;
        else if (layer > new_pos && l >= new_pos && l < layer) l += 1;
        kept[{ l, key.second }] = std::move(kvs);
    }
    doc->cell_properties = std::move(kept);
    ++doc->version;
    return true;
}
void je_tilemap_get_layer(je_TilemapDocument* doc, je_LayerId layer,
    /* OPTIONAL */ const char** name, /* OPTIONAL */ int32_t* visible, /* OPTIONAL */ int32_t* locked)
{
    const jeecs::Tilemap::MapLayer* l =
        layer >= 0 && layer < (int32_t)doc->layers.size()
        ? &doc->layers[layer] : nullptr;
    if (name != nullptr) *name = l ? l->name.c_str() : "";
    if (visible != nullptr) *visible = l ? (l->visible ? 1 : 0) : 0;
    if (locked != nullptr) *locked = l ? (l->locked ? 1 : 0) : 0;
}
void je_tilemap_set_layer(je_TilemapDocument* doc, je_LayerId layer,
    /* OPTIONAL */ const char* name, int32_t visible, int32_t locked)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size())
        return;
    auto& l = doc->layers[layer];
    bool changed = false;
    if (name != nullptr && l.name != name) { l.name = name; changed = true; }
    if (l.visible != (visible != 0)) { l.visible = visible != 0; changed = true; }
    if (l.locked != (locked != 0)) { l.locked = locked != 0; changed = true; }
    if (changed)
        ++doc->version;
}

// 图层的世界 z 偏移（相对地图根实体；不同层错开以避免深度冲突）
float je_tilemap_layer_z(je_TilemapDocument* doc, je_LayerId layer)
{
    return layer >= 0 && layer < (int32_t)doc->layers.size()
        ? doc->layers[layer].z : 0.f;
}
void je_tilemap_set_layer_z(je_TilemapDocument* doc, je_LayerId layer, float z)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size())
        return;
    if (doc->layers[layer].z != z)
    {
        doc->layers[layer].z = z;
        ++doc->version;
    }
}

// 图层的渲染队列值（同图各层按该值升序绘制；缺省 基数 + 层下标）
int32_t je_tilemap_layer_rend_queue(je_TilemapDocument* doc, je_LayerId layer)
{
    return layer >= 0 && layer < (int32_t)doc->layers.size()
        ? doc->layers[layer].rend_queue : 0;
}
void je_tilemap_set_layer_rend_queue(je_TilemapDocument* doc, je_LayerId layer, int32_t rend_queue)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size())
        return;
    if (doc->layers[layer].rend_queue != rend_queue)
    {
        doc->layers[layer].rend_queue = rend_queue;
        ++doc->version;
    }
}

// ---------------- 图层着色器样式 ----------------
// 样式修改只 bump 层内 style_version（不 bump doc->version）：样式与
// 网格内容正交，不应触发渲染侧的全网格重扫与顶点重建。
// 不透明指针 je_tilemap_shader_ref / je_tilemap_texture_ref 在此即
// basic::resource<graphic::shader/texture>*（见 include/jeecs.hpp）。
/* OPTIONAL: If layer has no shader style */ je_tilemap_shader_ref* je_tilemap_layer_shader(je_TilemapDocument* doc, je_LayerId layer)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size()
        || !doc->layers[layer].style.shader.has_value())
        return nullptr;
    return (je_tilemap_shader_ref*)new jeecs::basic::resource<jeecs::graphic::shader>(
        doc->layers[layer].style.shader.value());
}
void je_tilemap_set_layer_shader(je_TilemapDocument* doc, je_LayerId layer,
    /* OPTIONAL */ je_tilemap_shader_ref* shader)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size())
        return;
    auto& style = doc->layers[layer].style;
    auto* res = (jeecs::basic::resource<jeecs::graphic::shader>*)shader;
    if (res == nullptr)
    {
        if (style.shader.has_value())
        {
            style.shader.reset();
            ++style.style_version;
        }
        return;
    }
    if (!style.shader.has_value() || style.shader.value().get() != res->get())
    {
        style.shader.emplace(*res);
        ++style.style_version;
    }
}
int32_t je_tilemap_layer_texture_count(je_TilemapDocument* doc, je_LayerId layer)
{
    return layer >= 0 && layer < (int32_t)doc->layers.size()
        ? (int32_t)doc->layers[layer].style.textures.size() : 0;
}
bool je_tilemap_layer_set_texture(je_TilemapDocument* doc, je_LayerId layer,
    int32_t slot, /* OPTIONAL */ je_tilemap_texture_ref* texture)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size()
        || slot < 1)
        return false;
    auto& style = doc->layers[layer].style;
    auto fnd = std::find_if(style.textures.begin(), style.textures.end(),
        [slot](const jeecs::Tilemap::LayerTexture& t) { return t.slot == slot; });

    if (texture == nullptr)
    {
        if (fnd == style.textures.end())
            return false;
        style.textures.erase(fnd);
        ++style.style_version;
        return true;
    }
    auto* res = (jeecs::basic::resource<jeecs::graphic::texture2d>*)texture;
    if (fnd != style.textures.end())
    {
        if (fnd->texture.get() == res->get())
            return true;
        fnd->texture = *res;
    }
    else
        style.textures.push_back(jeecs::Tilemap::LayerTexture{ slot, *res });
    ++style.style_version;
    return true;
}
bool je_tilemap_layer_get_texture(je_TilemapDocument* doc, je_LayerId layer,
    int32_t index, /* OPTIONAL */ int32_t* slot, /* OPTIONAL */ je_tilemap_texture_ref** texture)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size()
        || index < 0 || index >= (int32_t)doc->layers[layer].style.textures.size())
        return false;
    const auto& t = doc->layers[layer].style.textures[index];
    if (slot != nullptr) *slot = t.slot;
    if (texture != nullptr)
        *texture = (je_tilemap_texture_ref*)
        new jeecs::basic::resource<jeecs::graphic::texture2d>(t.texture);
    return true;
}
void je_tilemap_free_shader_ref(/* OPTIONAL */ je_tilemap_shader_ref* shader)
{
    delete (jeecs::basic::resource<jeecs::graphic::shader>*)shader;
}
void je_tilemap_free_texture_ref(/* OPTIONAL */ je_tilemap_texture_ref* texture)
{
    delete (jeecs::basic::resource<jeecs::graphic::texture2d>*)texture;
}

// ---------------- 瓦片读写 ----------------
int32_t je_tilemap_get_tile(je_TilemapDocument* doc, je_LayerId layer, int32_t x, int32_t y)
{
    if (!doc->in_bounds(layer, x, y))
        return 0;
    return doc->layers[layer].grid[y * doc->width + x];
}
void je_tilemap_set_tile(je_TilemapDocument* doc, je_LayerId layer, int32_t x, int32_t y, int32_t value)
{
    if (!doc->in_bounds(layer, x, y))
        return;
    int32_t& cell = doc->layers[layer].grid[y * doc->width + x];
    if (cell != value)
    {
        cell = value;
        ++doc->version;
    }
}
void je_tilemap_fill_tiles(je_TilemapDocument* doc, je_LayerId layer,
    int32_t x, int32_t y, int32_t w, int32_t h, int32_t value)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size())
        return;
    bool changed = false;
    for (int32_t yy = y; yy < y + h; ++yy)
    {
        if (yy < 0 || yy >= doc->height) continue;
        for (int32_t xx = x; xx < x + w; ++xx)
        {
            if (xx < 0 || xx >= doc->width) continue;
            int32_t& cell = doc->layers[layer].grid[yy * doc->width + xx];
            if (cell != value) { cell = value; changed = true; }
        }
    }
    if (changed)
        ++doc->version;
}

// ---------------- 引用图集 ----------------
int32_t je_tilemap_tileset_count(je_TilemapDocument* doc)
{
    return (int32_t)doc->tilesets.size();
}
bool je_tilemap_add_tileset(je_TilemapDocument* doc, const char* path, int32_t* out_index)
{
    assert(path != nullptr);
    assert(out_index != nullptr);
    if (path[0] == '\0')
        return false;
    // 不要求图集此刻可解析（可先引用后保存图集）
    for (size_t i = 0; i < doc->tilesets.size(); ++i)
        if (doc->tilesets[i].path == path)
        {
            *out_index = (int32_t)i;
            return true;
        }
    jeecs::Tilemap::documents::inst().append_tileset_entry(*doc, path);
    ++doc->version;
    *out_index = (int32_t)doc->tilesets.size() - 1;
    return true;
}
bool je_tilemap_remove_tileset(je_TilemapDocument* doc, int32_t idx)
{
    if (idx < 0 || idx >= (int32_t)doc->tilesets.size())
        return false;
    jeecs::Tilemap::documents::inst().erase_tileset_entry(*doc, (size_t)idx);
    // 清除引用该图集的所有格子（编码随之失效）
    for (auto& layer : doc->layers)
        for (auto& v : layer.grid)
        {
            if (v == 0) continue;
            auto cell = jeecs::Tilemap::decode_tile_value(v);
            if (!cell.has_value())
                continue;
            if (cell->tileset_idx == idx)
                v = 0;
            else if (cell->tileset_idx > idx)
            {
                const int32_t moved = cell->tileset_idx - 1;
                v = cell->is_terrain
                    ? -jeecs::Tilemap::encode_terrain_value(moved, cell->local_id)
                    : jeecs::Tilemap::encode_tile_value(moved, cell->local_id);
            }
        }
    ++doc->version;
    return true;
}
const char* je_tilemap_tileset_path(je_TilemapDocument* doc, int32_t idx)
{
    if (idx < 0 || idx >= (int32_t)doc->tilesets.size())
        return "";
    return doc->tilesets[idx].path.c_str();
}

// ---------------- 属性与查询 ----------------
// void* <-> property_value 的按类型编组：je_PropertyType 与 variant 备选
// 下标一一对应，类型严格匹配（不匹配即失败，不做隐式转换）。数据形态
// 见 jeecs.hpp 中 je_PropertyType 的说明。
static bool je_tilemap_write_property(
    jeecs::Tilemap::property_value& out, je_PropertyType type, const void* value)
{
    assert(value != nullptr);
    using jeecs::Tilemap::property_value;
    switch (type)
    {
    case JE_PROPERTY_TYPE_BOOL:
        out = *static_cast<const bool*>(value);
        return true;
    case JE_PROPERTY_TYPE_STRING:
        out = property_value{ std::in_place_type<std::string>,
            std::string(static_cast<const char*>(value)) };
        return true;
    case JE_PROPERTY_TYPE_INT:
        out = *static_cast<const int32_t*>(value);
        return true;
    case JE_PROPERTY_TYPE_FLOAT:
        out = *static_cast<const float*>(value);
        return true;
    case JE_PROPERTY_TYPE_VEC2:
    {
        const float* f = static_cast<const float*>(value);
        out = std::array<float, 2>{ f[0], f[1] };
        return true;
    }
    case JE_PROPERTY_TYPE_VEC3:
    {
        const float* f = static_cast<const float*>(value);
        out = std::array<float, 3>{ f[0], f[1], f[2] };
        return true;
    }
    case JE_PROPERTY_TYPE_VEC4:
    {
        const float* f = static_cast<const float*>(value);
        out = std::array<float, 4>{ f[0], f[1], f[2], f[3] };
        return true;
    }
    }
    return false;
}
static bool je_tilemap_read_property(
    const jeecs::Tilemap::property_value& v, je_PropertyType type, void* out_value)
{
    assert(out_value != nullptr);
    switch (type)
    {
    case JE_PROPERTY_TYPE_BOOL:
        if (auto p = std::get_if<bool>(&v))
        {
            *static_cast<bool*>(out_value) = *p;
            return true;
        }
        break;
    case JE_PROPERTY_TYPE_STRING:
        if (auto p = std::get_if<std::string>(&v))
        {
            // 指向文档内部存储，下次修改该值前有效
            *static_cast<const char**>(out_value) = p->c_str();
            return true;
        }
        break;
    case JE_PROPERTY_TYPE_INT:
        if (auto p = std::get_if<int32_t>(&v))
        {
            *static_cast<int32_t*>(out_value) = *p;
            return true;
        }
        break;
    case JE_PROPERTY_TYPE_FLOAT:
        if (auto p = std::get_if<float>(&v))
        {
            *static_cast<float*>(out_value) = *p;
            return true;
        }
        break;
    case JE_PROPERTY_TYPE_VEC2:
        if (auto p = std::get_if<std::array<float, 2>>(&v))
        {
            memcpy(out_value, p->data(), sizeof(float) * 2);
            return true;
        }
        break;
    case JE_PROPERTY_TYPE_VEC3:
        if (auto p = std::get_if<std::array<float, 3>>(&v))
        {
            memcpy(out_value, p->data(), sizeof(float) * 3);
            return true;
        }
        break;
    case JE_PROPERTY_TYPE_VEC4:
        if (auto p = std::get_if<std::array<float, 4>>(&v))
        {
            memcpy(out_value, p->data(), sizeof(float) * 4);
            return true;
        }
        break;
    }
    return false;
}

bool je_tilemap_set_cell_property(je_TilemapDocument* doc, je_LayerId layer,
    int32_t x, int32_t y, const char* name, je_PropertyType type,
    /* OPTIONAL */ const void* value)
{
    assert(name != nullptr);
    if (!doc->in_bounds(layer, x, y)
        || !jeecs::Tilemap::property_type_valid((int32_t)type))
        return false;
    auto key = std::make_pair(layer, x + y * doc->width);
    if (value == nullptr)
    {
        // 清除覆盖：忽略 type，未设置过也视为成功
        auto fnd = doc->cell_properties.find(key);
        if (fnd != doc->cell_properties.end())
        {
            fnd->second.erase(name);
            if (fnd->second.empty())
                doc->cell_properties.erase(fnd);
            ++doc->version;
        }
        return true;
    }
    auto cell = doc->cell_properties.find(key);
    if (cell != doc->cell_properties.end())
    {
        auto fnd = cell->second.find(name);
        if (fnd != cell->second.end() && fnd->second.index() != (size_t)type)
            return false;   // 已有不同类型的值，拒绝改型
    }
    jeecs::Tilemap::property_value v;
    if (!je_tilemap_write_property(v, type, value))
        return false;
    doc->cell_properties[key][name] = std::move(v);
    ++doc->version;
    return true;
}
bool je_tilemap_get_cell_property(je_TilemapDocument* doc, je_LayerId layer,
    int32_t x, int32_t y, const char* name, je_PropertyType type,
    /* OPTIONAL */ void* out_value)
{
    assert(name != nullptr);
    assert(out_value != nullptr);
    if (!doc->in_bounds(layer, x, y))
        return false;
    auto fnd = doc->cell_properties.find({ layer, x + y * doc->width });
    if (fnd == doc->cell_properties.end())
        return false;
    auto vit = fnd->second.find(name);
    return vit == fnd->second.end()
        ? false
        : je_tilemap_read_property(vit->second, type, out_value);
}
bool je_tilemap_get_property(je_TilemapDocument* doc, je_LayerId layer,
    int32_t x, int32_t y, const char* name, je_PropertyType type,
    /* OPTIONAL */ void* out_value)
{
    assert(name != nullptr);
    assert(out_value != nullptr);
    const jeecs::Tilemap::property_value* r =
        jeecs::Tilemap::resolve_property(doc, layer, x, y, name);
    return r == nullptr ? false : je_tilemap_read_property(*r, type, out_value);
}
int32_t je_tilemap_is_walkable(je_TilemapDocument* doc, int32_t x, int32_t y)
{
    if (!doc->in_bounds(0, x, y))
        return 0;   // 出界视为不可通行
    for (int32_t l = 0; l < (int32_t)doc->layers.size(); ++l)
        if (!jeecs::Tilemap::walkable_on(doc, l, x, y))
            return 0;
    return 1;
}
int32_t je_tilemap_is_walkable_on(je_TilemapDocument* doc, je_LayerId layer, int32_t x, int32_t y)
{
    return jeecs::Tilemap::walkable_on(doc, layer, x, y) ? 1 : 0;
}
int32_t je_tilemap_find_cells(je_TilemapDocument* doc, je_LayerId layer,
    const char* name, je_PropertyType type, const void* value)
{
    assert(name != nullptr);
    assert(value != nullptr);
    auto& result = doc->find_result;
    result.clear();
    std::set<int32_t> matched;
    const bool walkable_query = strcmp(name, "walkable") == 0;
    bool walkable_flag = false;
    jeecs::Tilemap::property_value query;
    if (walkable_query)
    {
        if (type != JE_PROPERTY_TYPE_BOOL)
            return 0;
        walkable_flag = *static_cast<const bool*>(value);
    }
    else if (!jeecs::Tilemap::property_type_valid((int32_t)type)
        || !je_tilemap_write_property(query, type, value))
    {
        return 0;
    }
    auto try_layer = [&](int32_t l)
        {
            for (int32_t y = 0; y < doc->height; ++y)
                for (int32_t x = 0; x < doc->width; ++x)
                {
                    if (matched.count(x + y * doc->width) != 0)
                        continue;
                    bool hit = false;
                    if (walkable_query)
                    {
                        bool w = jeecs::Tilemap::walkable_on(doc, l, x, y);
                        hit = w == walkable_flag;
                    }
                    else
                    {
                        const auto* r = jeecs::Tilemap::resolve_property(doc, l, x, y, name);
                        hit = r != nullptr && *r == query;
                    }
                    if (hit)
                    {
                        matched.insert(x + y * doc->width);
                        result.push_back({ x, y });
                    }
                }
        };
    if (layer < 0)
    {
        for (int32_t l = 0; l < (int32_t)doc->layers.size(); ++l)
            try_layer(l);
    }
    else if (layer < (int32_t)doc->layers.size())
        try_layer(layer);
    return (int32_t)result.size();
}
bool je_tilemap_find_get(je_TilemapDocument* doc, int32_t index, int32_t* x, int32_t* y)
{
    assert(x != nullptr);
    assert(y != nullptr);
    const auto& result = doc->find_result;
    if (index < 0 || index >= (int32_t)result.size())
        return false;
    *x = result[index].first;
    *y = result[index].second;
    return true;
}

// ---------------- 图集文档 ----------------
/* OPTIONAL: If path cannot be opened */ je_TilesetDocument* je_tilemap_open_tileset(const char* path)
{
    assert(path != nullptr);
    // open_tileset 返回自带 1 个外部引用的实例（unload/gchandle 回收时
    // 释放）；打开失败（文件缺失/损坏，含已标记失败不重试）为 nullptr
    auto doc = jeecs::Tilemap::documents::inst().open_tileset(path);
    return doc.has_value() ? doc->get() : nullptr;
}
je_TilesetDocument* je_tilemap_create_tileset(const char* name)
{
    assert(name != nullptr);
    auto doc = std::make_shared<jeecs::Tilemap::TilesetDocument>();
    doc->name = name;
    doc->tiles.push_back({});
    doc->terrains.push_back({});
    // register_tileset 返回自带 1 个引用的实例
    return jeecs::Tilemap::documents::inst().register_tileset(std::move(doc));
}
int32_t je_tilemap_keep_tileset(je_TilesetDocument* tileset)
{
    return jeecs::Tilemap::documents::inst().keep_tileset(tileset);
}
int32_t je_tilemap_unload_tileset(je_TilesetDocument* tileset)
{
    return jeecs::Tilemap::documents::inst().unload_tileset(tileset);
}
bool je_tilemap_save_tileset(je_TilesetDocument* tileset, const char* path)
{
    assert(path != nullptr);
    return jeecs::Tilemap::documents::inst().save_tileset(tileset, path);
}
void je_tilemap_reload_tileset(const char* path)
{
    assert(path != nullptr);
    jeecs::Tilemap::documents::inst().reload_tileset(path);
}
uint64_t je_tilemap_tileset_version(je_TilesetDocument* doc)
{
    return doc->version;
}
const char* je_tilemap_tileset_doc_path(je_TilesetDocument* doc)
{
    return doc->path.c_str();
}
void je_tilemap_tileset_set_name(je_TilesetDocument* doc, const char* name)
{
    assert(name != nullptr);
    doc->name = name;
    ++doc->version;
}
const char* je_tilemap_tileset_name(je_TilesetDocument* doc)
{
    return doc->name.c_str();
}

// ---------------- 图集源纹理 ----------------
bool je_tilemap_add_source(je_TilesetDocument* doc, const char* texture_path,
    int32_t tile_px, je_SourceId* out_id)
{
    assert(texture_path != nullptr);
    assert(out_id != nullptr);
    if (texture_path[0] == '\0'
        || tile_px <= 0 || tile_px > 4096)
        return false;
    // 复用已有的同路径源
    for (size_t i = 0; i < doc->sources.size(); ++i)
        if (doc->sources[i].path == texture_path)
        {
            *out_id = (je_SourceId)i;
            return true;
        }
    // 读取纹理尺寸以推导网格数
    int32_t tw = 0, th = 0;
    if (auto tex = jeecs::graphic::texture2d::load(nullptr, texture_path))
    {
        tw = (int32_t)tex->get()->resource()->m_width;
        th = (int32_t)tex->get()->resource()->m_height;
    }
    else
    {
        jeecs::debug::logerr("Tilemap: unable to load tileset texture '%s'.", texture_path);
        return false;
    }
    if (tw < tile_px || th < tile_px)
        return false;
    jeecs::Tilemap::SourceTexture src;
    src.path = texture_path;
    src.tile_px = tile_px;
    src.xcount = tw / tile_px;
    src.ycount = th / tile_px;
    src.tex_w = tw;
    src.tex_h = th;
    // 展开为"每层一个瓦片"的数组纹理（分片采样用）
    if (!jeecs::Tilemap::build_source_array(src))
        jeecs::debug::logwarn(
            "Tilemap: texture array for '%s' deferred (image not ready).",
            texture_path);
    doc->sources.push_back(std::move(src));
    ++doc->version;
    *out_id = (je_SourceId)doc->sources.size() - 1;
    return true;
}
bool je_tilemap_remove_source(je_TilesetDocument* doc, je_SourceId source_idx)
{
    if (source_idx < 0 || source_idx >= (int32_t)doc->sources.size())
        return false;
    doc->sources.erase(doc->sources.begin() + source_idx);
    // 引用该源的瓦片/地形做墓碑（source 置空），其余重排 source 下标。
    // 与 remove_terrain 同理：不移动数组元素，地图按 id 引用永不漂移。
    for (auto& d : doc->tiles)
    {
        if (d.source == source_idx) { d.source = std::nullopt; continue; }
        if (d.source > source_idx) d.source = *d.source - 1;
    }
    for (auto& d : doc->terrains)
    {
        if (d.source == source_idx)
        {
            d.source = std::nullopt;
            d.name.clear();
            d.ix = 0; d.iy = 0;
            d.variant_tiles.clear();
            d.properties.clear();
            continue;
        }
        if (d.source > source_idx) d.source = *d.source - 1;
    }
    doc->rebuild_flat_index();
    ++doc->version;
    return true;
}
int32_t je_tilemap_source_count(je_TilesetDocument* doc)
{
    return (int32_t)doc->sources.size();
}
bool je_tilemap_source_info(je_TilesetDocument* doc, je_SourceId source_idx,
    /* OPTIONAL */ const char** texture_path, /* OPTIONAL */ int32_t* tile_px, /* OPTIONAL */ int32_t* xcount, /* OPTIONAL */ int32_t* ycount)
{
    if (source_idx < 0 || source_idx >= (int32_t)doc->sources.size())
        return false;
    const auto& s = doc->sources[source_idx];
    if (texture_path != nullptr) *texture_path = s.path.c_str();
    if (tile_px != nullptr) *tile_px = s.tile_px;
    if (xcount != nullptr) *xcount = s.xcount;
    if (ycount != nullptr) *ycount = s.ycount;
    return true;
}

// ---------------- 普通瓦片表 ----------------
bool je_tilemap_add_tile(je_TilesetDocument* doc, je_SourceId source_idx,
    int32_t ix, int32_t iy, int32_t walkable, je_TileId* out_id)
{
    assert(out_id != nullptr);
    if (source_idx < 0 || source_idx >= (int32_t)doc->sources.size()
        || ix < 0 || iy < 0
        || ix >= doc->sources[source_idx].xcount || iy >= doc->sources[source_idx].ycount)
        return false;
    // 复用已有同位置瓦片
    for (size_t i = 1; i < doc->tiles.size(); ++i)
        if (doc->tiles[i].source == source_idx
            && doc->tiles[i].ix == ix && doc->tiles[i].iy == iy)
        {
            *out_id = (je_TileId)i;
            return true;
        }
    jeecs::Tilemap::TileDef t;
    t.source = source_idx; t.ix = ix; t.iy = iy;
    t.walkable = walkable != 0;
    doc->tiles.push_back(t);
    doc->rebuild_flat_index();
    ++doc->version;
    *out_id = (je_TileId)doc->tiles.size() - 1;
    return true;
}
bool je_tilemap_remove_tile(je_TilesetDocument* doc, je_TileId tile_id)
{
    if (tile_id <= 0 || tile_id >= (int32_t)doc->tiles.size())
        return false;
    // 墓碑删除（同 remove_terrain）：地图按 id 引用瓦片，不移动数组，
    // id 永不漂移，跨会话安全。
    auto& t = doc->tiles[tile_id];
    if (!t.source.has_value())
        return false;
    t.source = std::nullopt;
    t.ix = 0; t.iy = 0;
    doc->tile_properties.erase(tile_id);
    doc->rebuild_flat_index();
    ++doc->version;
    return true;
}
int32_t je_tilemap_tile_count(je_TilesetDocument* doc)
{
    return (int32_t)doc->tiles.size() - 1;
}
bool je_tilemap_tile_info(je_TilesetDocument* doc, je_TileId tile_id,
    /* OPTIONAL */ je_SourceId* source_idx, /* OPTIONAL */ int32_t* ix, /* OPTIONAL */ int32_t* iy, /* OPTIONAL */ int32_t* walkable)
{
    if (tile_id <= 0 || tile_id >= (int32_t)doc->tiles.size())
        return false;
    const auto& t = doc->tiles[tile_id];
    // 已删除（墓碑）的瓦片经 source_idx 出参报告为 -1（C ABI 边界约定）
    if (source_idx != nullptr) *source_idx = t.source.value_or(-1);
    if (ix != nullptr) *ix = t.ix;
    if (iy != nullptr) *iy = t.iy;
    if (walkable != nullptr) *walkable = t.walkable ? 1 : 0;
    return true;
}
void je_tilemap_set_tile_walkable(je_TilesetDocument* doc, je_TileId tile_id, int32_t walkable)
{
    if (tile_id <= 0 || tile_id >= (int32_t)doc->tiles.size())
        return;
    doc->tiles[tile_id].walkable = walkable != 0;
    ++doc->version;
}

// ---------------- 自动图块地形 ----------------
bool je_tilemap_add_terrain(je_TilesetDocument* doc, const char* name, je_TerrainKind kind,
    je_SourceId source_idx, int32_t ix, int32_t iy, int32_t walkable, je_TerrainId* out_id)
{
    assert(name != nullptr);
    assert(out_id != nullptr);
    if (kind < JE_TERRAIN_KIND_SINGLE || kind > JE_TERRAIN_KIND_RPGMAKER_VX
        || source_idx < 0 || source_idx >= (int32_t)doc->sources.size()
        || ix < 0 || iy < 0
        || ix >= doc->sources[source_idx].xcount || iy >= doc->sources[source_idx].ycount)
        return false;
    jeecs::Tilemap::TerrainDef t;
    t.name = name;
    t.kind = kind;
    t.source = source_idx;
    t.ix = ix; t.iy = iy;
    t.walkable = walkable != 0;
    doc->terrains.push_back(std::move(t));
    ++doc->version;
    *out_id = (int32_t)doc->terrains.size() - 1;
    return true;
}
bool je_tilemap_remove_terrain(je_TilesetDocument* doc, je_TerrainId terrain_id)
{
    if (terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size())
        return false;
    // 墓碑删除：只标记不移动数组。地图格子按 id 引用地形，若抹除元素，
    // 其后所有地形 id 前移，已保存地图（可能在其他会话中绘制）将整体
    // 错位。标记为 source 置空（渲染/查询自然失效），id 永不复用。
    auto& t = doc->terrains[terrain_id];
    if (!t.source.has_value())
        return false;
    t.source = std::nullopt;
    t.name.clear();
    t.ix = 0; t.iy = 0;
    t.variant_tiles.clear();
    t.properties.clear();
    ++doc->version;
    return true;
}
int32_t je_tilemap_terrain_count(je_TilesetDocument* doc)
{
    return (int32_t)doc->terrains.size() - 1;
}
bool je_tilemap_terrain_info(je_TilesetDocument* doc, je_TerrainId terrain_id,
    /* OPTIONAL */ const char** name, /* OPTIONAL */ je_TerrainKind* kind, /* OPTIONAL */ je_SourceId* source_idx, /* OPTIONAL */ int32_t* ix, /* OPTIONAL */ int32_t* iy,
    /* OPTIONAL */ int32_t* walkable, /* OPTIONAL */ int32_t* variant_count)
{
    if (terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size())
        return false;
    const auto& t = doc->terrains[terrain_id];
    if (name != nullptr) *name = t.name.c_str();
    if (kind != nullptr) *kind = t.kind;
    // 已删除（墓碑）的地形经 source_idx 出参报告为 -1（C ABI 边界约定）
    if (source_idx != nullptr) *source_idx = t.source.value_or(-1);
    if (ix != nullptr) *ix = t.ix;
    if (iy != nullptr) *iy = t.iy;
    if (walkable != nullptr) *walkable = t.walkable ? 1 : 0;
    if (variant_count != nullptr) *variant_count = (int32_t)t.variant_tiles.size();
    return true;
}
bool je_tilemap_set_terrain(je_TilesetDocument* doc, je_TerrainId terrain_id,
    /* OPTIONAL */ const char* name, je_TerrainKind kind,
    je_SourceId source_idx, int32_t ix, int32_t iy, int32_t walkable)
{
    if (terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size()
        || kind < JE_TERRAIN_KIND_SINGLE || kind > JE_TERRAIN_KIND_RPGMAKER_VX
        || source_idx < 0 || source_idx >= (int32_t)doc->sources.size()
        || ix < 0 || iy < 0
        || ix >= doc->sources[source_idx].xcount || iy >= doc->sources[source_idx].ycount)
        return false;
    // 墓碑槽位不可编辑（防止复活已删除 id 造成旧引用错位）
    if (!doc->terrains[terrain_id].source.has_value())
        return false;
    auto& t = doc->terrains[terrain_id];
    if (name != nullptr) t.name = name;
    t.kind = kind;
    t.source = source_idx;
    t.ix = ix; t.iy = iy;
    t.walkable = walkable != 0;
    ++doc->version;
    return true;
}
void je_tilemap_terrain_clear_variants(je_TilesetDocument* doc, je_TerrainId terrain_id)
{
    if (terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size())
        return;
    if (!doc->terrains[terrain_id].variant_tiles.empty())
    {
        doc->terrains[terrain_id].variant_tiles.clear();
        ++doc->version;
    }
}
bool je_tilemap_terrain_add_variant(je_TilesetDocument* doc, je_TerrainId terrain_id, je_TileId tile_id)
{
    if (terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size()
        || tile_id <= 0 || tile_id >= (int32_t)doc->tiles.size())
        return false;
    doc->terrains[terrain_id].variant_tiles.push_back(tile_id);
    ++doc->version;
    return true;
}

// ---------------- 属性 schema 与默认值 ----------------
bool je_tilemap_add_property(je_TilesetDocument* doc, const char* name,
    je_PropertyType type, /* OPTIONAL */ const void* default_value, int32_t* out_index)
{
    assert(name != nullptr);
    assert(out_index != nullptr);
    if (name[0] == '\0' || !jeecs::Tilemap::property_type_valid((int32_t)type))
        return false;
    for (const auto& pd : doc->properties)
        if (pd.name == name)
            return false;
    jeecs::Tilemap::PropertyDef pd;
    pd.name = name;
    pd.type = type;
    if (default_value != nullptr)
    {
        if (!je_tilemap_write_property(pd.default_value, type, default_value))
            return false;
    }
    else
        pd.default_value = jeecs::Tilemap::property_zero_value(type);
    doc->properties.push_back(std::move(pd));
    ++doc->version;
    *out_index = (int32_t)doc->properties.size() - 1;
    return true;
}
bool je_tilemap_remove_property(je_TilesetDocument* doc, const char* name)
{
    assert(name != nullptr);
    for (size_t i = 0; i < doc->properties.size(); ++i)
        if (doc->properties[i].name == name)
        {
            doc->properties.erase(doc->properties.begin() + i);
            ++doc->version;
            return true;
        }
    return false;
}
int32_t je_tilemap_property_count(je_TilesetDocument* doc)
{
    return (int32_t)doc->properties.size();
}
bool je_tilemap_property_info(je_TilesetDocument* doc, int32_t index,
    /* OPTIONAL */ const char** name, /* OPTIONAL */ je_PropertyType* type)
{
    if (index < 0 || index >= (int32_t)doc->properties.size())
        return false;
    const auto& pd = doc->properties[index];
    if (name != nullptr) *name = pd.name.c_str();
    if (type != nullptr) *type = pd.type;
    return true;
}
bool je_tilemap_property_default(je_TilesetDocument* doc, const char* name,
    je_PropertyType type, /* OPTIONAL */ void* out_value)
{
    assert(name != nullptr);
    assert(out_value != nullptr);
    for (const auto& pd : doc->properties)
        if (pd.name == name)
            return je_tilemap_read_property(pd.default_value, type, out_value);
    return false;
}
bool je_tilemap_set_tile_property(je_TilesetDocument* doc, je_TileId tile_id,
    const char* name, je_PropertyType type, /* OPTIONAL */ const void* value)
{
    assert(name != nullptr);
    if (tile_id <= 0 || tile_id >= (int32_t)doc->tiles.size()
        || !jeecs::Tilemap::property_type_valid((int32_t)type))
        return false;
    if (value == nullptr)
    {
        // 清除默认值：忽略 type，未设置过也视为成功
        auto fnd = doc->tile_properties.find(tile_id);
        if (fnd != doc->tile_properties.end())
        {
            fnd->second.erase(name);
            if (fnd->second.empty())
                doc->tile_properties.erase(fnd);
            ++doc->version;
        }
        return true;
    }
    auto& props = doc->tile_properties[tile_id];
    auto fnd = props.find(name);
    if (fnd != props.end() && fnd->second.index() != (size_t)type)
        return false;   // 已有不同类型的值，拒绝改型
    jeecs::Tilemap::property_value v;
    if (!je_tilemap_write_property(v, type, value))
        return false;
    props[name] = std::move(v);
    ++doc->version;
    return true;
}
bool je_tilemap_get_tile_property(je_TilesetDocument* doc, je_TileId tile_id,
    const char* name, je_PropertyType type, /* OPTIONAL */ void* out_value)
{
    assert(name != nullptr);
    assert(out_value != nullptr);
    if (tile_id <= 0 || tile_id >= (int32_t)doc->tiles.size())
        return false;
    auto fnd = doc->tile_properties.find(tile_id);
    if (fnd == doc->tile_properties.end())
        return false;
    auto vit = fnd->second.find(name);
    return vit == fnd->second.end()
        ? false
        : je_tilemap_read_property(vit->second, type, out_value);
}
bool je_tilemap_set_terrain_property(je_TilesetDocument* doc, je_TerrainId terrain_id,
    const char* name, je_PropertyType type, /* OPTIONAL */ const void* value)
{
    assert(name != nullptr);
    if (terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size()
        || !jeecs::Tilemap::property_type_valid((int32_t)type))
        return false;
    auto& props = doc->terrains[terrain_id].properties;
    if (value == nullptr)
    {
        if (props.erase(name) > 0)
            ++doc->version;
        return true;
    }
    auto fnd = props.find(name);
    if (fnd != props.end() && fnd->second.index() != (size_t)type)
        return false;   // 已有不同类型的值，拒绝改型
    jeecs::Tilemap::property_value v;
    if (!je_tilemap_write_property(v, type, value))
        return false;
    props[name] = std::move(v);
    ++doc->version;
    return true;
}
bool je_tilemap_get_terrain_property(je_TilesetDocument* doc, je_TerrainId terrain_id,
    const char* name, je_PropertyType type, /* OPTIONAL */ void* out_value)
{
    assert(name != nullptr);
    assert(out_value != nullptr);
    if (terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size())
        return false;
    auto fnd = doc->terrains[terrain_id].properties.find(name);
    return fnd == doc->terrains[terrain_id].properties.end()
        ? false
        : je_tilemap_read_property(fnd->second, type, out_value);
}

// ======================================================================
// 单元格渲染信息（编辑器预览用）：resolve_cell_quads 的 C 边界映射
// ======================================================================
int32_t je_tilemap_cell_quad_count(je_TilemapDocument* doc, je_LayerId layer, int32_t x, int32_t y)
{
    jeecs::Tilemap::cell_quad_info quads[4];
    return jeecs::Tilemap::resolve_cell_quads(*doc, layer, x, y, quads);
}
bool je_tilemap_cell_quad(je_TilemapDocument* doc, je_LayerId layer, int32_t x, int32_t y, int32_t idx,
    /* OPTIONAL */ const char** texture_path,
    /* OPTIONAL */ float* u0, /* OPTIONAL */ float* v0, /* OPTIONAL */ float* u1, /* OPTIONAL */ float* v1,
    /* OPTIONAL */ int32_t* qx, /* OPTIONAL */ int32_t* qy, /* OPTIONAL */ int32_t* qcols)
{
    if (idx < 0 || idx > 3)
        return false;
    jeecs::Tilemap::cell_quad_info quads[4];
    int32_t count = jeecs::Tilemap::resolve_cell_quads(*doc, layer, x, y, quads);
    if (idx >= count)
        return false;

    const auto& q = quads[idx];
    if (q.tileset_idx < 0 || q.tileset_idx >= (int32_t)doc->tilesets.size()
        || !doc->tilesets[q.tileset_idx].resolved.has_value()
        || q.source_idx < 0 || q.source_idx >= (int32_t)doc->tilesets[q.tileset_idx].resolved->get()->sources.size())
        return false;

    if (texture_path != nullptr)
        *texture_path = doc->tilesets[q.tileset_idx].resolved->get()->sources[q.source_idx].path.c_str();

    // 编辑器预览按 2D 图集绘制：把 tile-local UV 换算回图集绝对 UV
    //（层号 -> 网格坐标：layer = iy * xcount + ix）
    const auto& src = doc->tilesets[q.tileset_idx].resolved->get()->sources[q.source_idx];
    const int32_t ix = q.layer % src.xcount;
    const int32_t iy = q.layer / src.xcount;
    const float w = (float)src.tex_w.value_or(src.xcount * src.tile_px);
    const float h = (float)src.tex_h.value_or(src.ycount * src.tile_px);
    const float px0 = (float)(ix * src.tile_px);
    const float py0 = (float)(iy * src.tile_px);
    const float tp = (float)src.tile_px;
    if (u0 != nullptr) *u0 = (px0 + q.u0 * tp) / w;
    if (v0 != nullptr) *v0 = 1.f - (py0 + (1.f - q.v0) * tp) / h;
    if (u1 != nullptr) *u1 = (px0 + q.u1 * tp) / w;
    if (v1 != nullptr) *v1 = 1.f - (py0 + (1.f - q.v1) * tp) / h;
    // 象限自左上角起于 qcols×qcols 网格：corner 0..3 -> (0,0)(1,0)(0,1)(1,1)
    if (qx != nullptr) *qx = q.corner.value_or(0) & 1;
    if (qy != nullptr) *qy = q.corner.value_or(0) >> 1;
    if (qcols != nullptr) *qcols = q.corner.has_value() ? 2 : 1;
    return true;
}

// ======================================================================
// wojeapi_tilemap_*：Woolang 绑定（je/tilemap.wo 经 extern 声明引用）
// 文档在 Woolang 侧为 gchandle：载荷即文档实例指针，值被回收（GC 或
// gchandle::close）时经终结器 unload 释放其携带的 1 个引用。
// ======================================================================

// 读取第 slot 个参数（文档 gchandle）的载荷实例指针，按文档族具体化
static inline je_TilemapDocument* woje_map_doc(woort_StackValue slot)
{
    return (je_TilemapDocument*)woort_gcpointer(slot);
}
static inline je_TilesetDocument* woje_ts_doc(woort_StackValue slot)
{
    return (je_TilesetDocument*)woort_gcpointer(slot);
}
// 终结器：释放 gchandle 携带的文档引用。注意 Woolang 为并发 GC——
// 终结器在 GC worker 线程上执行，可能与编辑器线程、世界更新线程同时
// 调用 unload（注册表内部已用互斥锁串行化；调用方 bug 导致的双重释放
// 由引用计数下限 0 与已逐出防护兜底）。
static void woje_tilemap_release_map(void* ptr)
{
    je_tilemap_unload_map((je_TilemapDocument*)ptr);
}
static void woje_tilemap_release_tileset(void* ptr)
{
    je_tilemap_unload_tileset((je_TilesetDocument*)ptr);
}

WOORT_API woort_api wojeapi_tilemap_open_map(void)
{
    je_TilemapDocument* doc = je_tilemap_open_map(woort_string(0));
    if (doc == nullptr)
        return woort_ret_option_none();
    return woort_ret_option_gchandle(
        doc, WOORT_IGNORE, woje_tilemap_release_map, nullptr);
}
WOORT_API woort_api wojeapi_tilemap_create_map(void)
{
    je_TilemapDocument* doc = je_tilemap_create_map(
        (int32_t)woort_int(0), (int32_t)woort_int(1), (int32_t)woort_int(2));
    if (doc == nullptr)
        return woort_ret_option_none();
    return woort_ret_option_gchandle(
        doc, WOORT_IGNORE, woje_tilemap_release_map, nullptr);
}
WOORT_API woort_api wojeapi_tilemap_save_map(void)
{
    return woort_ret_bool(je_tilemap_save_map(
        woje_map_doc(0), woort_string(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_reload_map(void)
{
    je_tilemap_reload_map(woort_string(0));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_map_size(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    int32_t w = 0, h = 0, tpx = 0, lc = 0;
    je_tilemap_map_size(woje_map_doc(0), &w, &h, &tpx, &lc);
    woort_set_struct(s + 0, 4);
    woort_struct_set_int(s + 0, 0, w);
    woort_struct_set_int(s + 0, 1, h);
    woort_struct_set_int(s + 0, 2, tpx);
    woort_struct_set_int(s + 0, 3, lc);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_map_path(void)
{
    const char* path;
    if (je_tilemap_map_path(woje_map_doc(0), &path))
        return woort_ret_option_string(path);

    return woort_ret_option_none();
}
WOORT_API woort_api wojeapi_tilemap_map_version(void)
{
    return woort_ret_int((woort_Int)je_tilemap_map_version(woje_map_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_loaded_maps(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value out_array = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(out_array);

    // 快照枚举：每个实例自带 1 个引用（收集与计引用同锁完成，不会被
    // GC 终结器并发逐出），元素被 GC 回收时经终结器释放
    for (je_TilemapDocument* doc :
        jeecs::Tilemap::documents::inst().snapshot_maps())
    {
        woort_set_gchandle(elem, doc, WOORT_IGNORE,
            woje_tilemap_release_map, nullptr);
        woort_vec_push(out_array, elem);
    }
    return woort_ret_value(out_array);
}
WOORT_API woort_api wojeapi_tilemap_resize_map(void)
{
    je_tilemap_resize_map(woje_map_doc(0), (int32_t)woort_int(1), (int32_t)woort_int(2));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_add_layer(void)
{
    je_LayerId id = 0;
    if (!je_tilemap_add_layer(woje_map_doc(0), woort_string(1), &id))
        return woort_ret_option_none();
    return woort_ret_option_int(id);
}
WOORT_API woort_api wojeapi_tilemap_remove_layer(void)
{
    return woort_ret_bool(je_tilemap_remove_layer(
        woje_map_doc(0), (int32_t)woort_int(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_move_layer(void)
{
    return woort_ret_bool(je_tilemap_move_layer(
        woje_map_doc(0), (int32_t)woort_int(1), (int32_t)woort_int(2)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_get_layer(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    const char* name = "";
    int32_t visible = 0, locked = 0;
    je_tilemap_get_layer(woje_map_doc(0), (int32_t)woort_int(1),
        &name, &visible, &locked);
    woort_set_struct(s + 0, 3);
    woort_struct_set_string(s + 0, 0, name);
    woort_struct_set_int(s + 0, 1, visible);
    woort_struct_set_int(s + 0, 2, locked);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_set_layer(void)
{
    je_tilemap_set_layer(woje_map_doc(0), (int32_t)woort_int(1),
        woort_string(2), (int32_t)woort_int(3), (int32_t)woort_int(4));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_layer_z(void)
{
    return woort_ret_float(je_tilemap_layer_z(
        woje_map_doc(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_set_layer_z(void)
{
    je_tilemap_set_layer_z(woje_map_doc(0), (int32_t)woort_int(1),
        woort_float(2));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_layer_rend_queue(void)
{
    return woort_ret_int(je_tilemap_layer_rend_queue(
        woje_map_doc(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_set_layer_rend_queue(void)
{
    je_tilemap_set_layer_rend_queue(woje_map_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_layer_shader(void)
{
    je_tilemap_shader_ref* shader = je_tilemap_layer_shader(
        woje_map_doc(0), (int32_t)woort_int(1));
    if (shader == nullptr)
        return woort_ret_option_none();
    // 新建引用副本交由 GC 释放（与 wojeapi_shader_open 同款）
    return woort_ret_option_gchandle(
        shader, WOORT_IGNORE,
        [](void* ptr)
        {
            delete (jeecs::basic::resource<jeecs::graphic::shader> *)ptr;
        },
        nullptr);
}
WOORT_API woort_api wojeapi_tilemap_set_layer_shader(void)
{
    // 第 2 参 shader 为 option<shader>：none 表示清除样式
    woort_value s;
    je_tilemap_shader_ref* shader = nullptr;
    if (woort_push_reserve(1, &s) && woort_option_get(s, 2))
        shader = (je_tilemap_shader_ref*)woort_gcpointer(s);
    je_tilemap_set_layer_shader(
        woje_map_doc(0), (int32_t)woort_int(1), shader);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_layer_texture_count(void)
{
    return woort_ret_int(je_tilemap_layer_texture_count(
        woje_map_doc(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_layer_set_texture(void)
{
    // 第 3 参 texture 为 option<texture>：none 表示删除该槽绑定
    woort_value s;
    je_tilemap_texture_ref* texture = nullptr;
    if (woort_push_reserve(1, &s) && woort_option_get(s, 3))
        texture = (je_tilemap_texture_ref*)woort_gcpointer(s);
    return woort_ret_bool(je_tilemap_layer_set_texture(
        woje_map_doc(0), (int32_t)woort_int(1), (int32_t)woort_int(2),
        texture) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_layer_get_texture(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");
    int32_t slot = 0;
    je_tilemap_texture_ref* texture = nullptr;
    je_tilemap_layer_get_texture(woje_map_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), &slot, &texture);
    woort_set_struct(s + 0, 2);
    woort_struct_set_int(s + 0, 0, slot);
    if (texture != nullptr)
        woort_set_option_gchandle(
            s + 1, texture, WOORT_IGNORE,
            [](void* ptr)
            {
                delete (jeecs::basic::resource<jeecs::graphic::texture2d> *)ptr;
            },
            nullptr);
    else
        woort_set_option_none(s + 1);
    woort_struct_set(s + 0, 1, s + 1);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_get_tile(void)
{
    return woort_ret_int(je_tilemap_get_tile(
        woje_map_doc(0), (int32_t)woort_int(1), (int32_t)woort_int(2), (int32_t)woort_int(3)));
}
WOORT_API woort_api wojeapi_tilemap_set_tile(void)
{
    je_tilemap_set_tile(woje_map_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3), (int32_t)woort_int(4));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_fill_tiles(void)
{
    je_tilemap_fill_tiles(woje_map_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3),
        (int32_t)woort_int(4), (int32_t)woort_int(5), (int32_t)woort_int(6));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_tileset_count(void)
{
    return woort_ret_int(je_tilemap_tileset_count(woje_map_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_add_tileset(void)
{
    int32_t index = 0;
    if (!je_tilemap_add_tileset(woje_map_doc(0), woort_string(1), &index))
        return woort_ret_option_none();
    return woort_ret_option_int(index);
}
WOORT_API woort_api wojeapi_tilemap_remove_tileset(void)
{
    return woort_ret_bool(je_tilemap_remove_tileset(
        woje_map_doc(0), (int32_t)woort_int(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_tileset_path(void)
{
    return woort_ret_string(je_tilemap_tileset_path(
        woje_map_doc(0), (int32_t)woort_int(1)));
}
// ---- 属性值的 Woolang 编组 ----
// 属性接口按 je_PropertyType 拆成每类型一个 extern（Woolang 侧用泛型
// 封装按值的静态类型分发，与 shader::set_uniform 同款做法）。
// 各宏中 s/pv 由使用点声明：PROPVAL_* 从 option<T> 参数解出值指针
//（none → nullptr，即清除/零默认）；SETTER/GETTER/ADDER/FINDER 生成
// 单个 extern 的完整函数体。
#define JE_WO_PROPVAL_BOOL(SLOT)                                       \
    bool propv = false;                                                \
    if (woort_push_reserve(1, &s) && woort_option_get(s, SLOT))        \
    {                                                                  \
        propv = woort_bool(s);                                         \
        pv = &propv;                                                   \
    }
#define JE_WO_PROPVAL_INT(SLOT)                                        \
    int32_t propv = 0;                                                 \
    if (woort_push_reserve(1, &s) && woort_option_get(s, SLOT))        \
    {                                                                  \
        propv = (int32_t)woort_int(s);                                 \
        pv = &propv;                                                   \
    }
#define JE_WO_PROPVAL_REAL(SLOT)                                       \
    float propv = 0.f;                                                 \
    if (woort_push_reserve(1, &s) && woort_option_get(s, SLOT))        \
    {                                                                  \
        propv = (float)woort_real(s);                                  \
        pv = &propv;                                                   \
    }
#define JE_WO_PROPVAL_STRING(SLOT)                                     \
    const char* propv = nullptr;                                       \
    if (woort_push_reserve(1, &s) && woort_option_get(s, SLOT))        \
    {                                                                  \
        propv = woort_string(s);                                       \
        pv = propv;                                                    \
    }
#define JE_WO_PROPVAL_VEC(SLOT, N)                                     \
    float propv[N] = { 0 };                                            \
    if (woort_push_reserve(1, &s) && woort_option_get(s, SLOT))        \
    {                                                                  \
        for (size_t i = 0; i < N; ++i)                                 \
            propv[i] = (float)woort_struct_get_real(s, i);             \
        pv = propv;                                                    \
    }
#define JE_WO_PROPVAL_VEC2(SLOT) JE_WO_PROPVAL_VEC(SLOT, 2)
#define JE_WO_PROPVAL_VEC3(SLOT) JE_WO_PROPVAL_VEC(SLOT, 3)
#define JE_WO_PROPVAL_VEC4(SLOT) JE_WO_PROPVAL_VEC(SLOT, 4)

// set 型：option<T> 值（nullptr = 清除）→ bool
#define JE_WO_PROP_SETTER(FN, CALL, SLOT, PROPVAL)                     \
WOORT_API woort_api FN(void)                                           \
{                                                                      \
    woort_value s;                                                     \
    const void* pv = nullptr;                                          \
    PROPVAL(SLOT)                                                      \
    return woort_ret_bool(CALL(pv) ? true : false);                    \
}
// get 型：C 调用成功（bool）→ option<T>；vecN 打包为 real 元组
#define JE_WO_PROP_GETTER(FN, CALL, CTYPE, INIT, RETOPT)               \
WOORT_API woort_api FN(void)                                           \
{                                                                      \
    CTYPE propv INIT;                                                  \
    if (CALL(&propv))                                                  \
        return RETOPT(propv);                                          \
    return woort_ret_option_none();                                    \
}
#define JE_WO_PROP_VECGETTER(FN, CALL, N)                              \
WOORT_API woort_api FN(void)                                           \
{                                                                      \
    float propv[N] = { 0 };                                            \
    if (CALL(propv))                                                   \
    {                                                                  \
        woort_value s;                                                 \
        if (!woort_push_reserve(1, &s))                                \
            return woort_ret_panic("Stack overflow.");                 \
        woort_set_struct(s + 0, N);                                    \
        for (size_t i = 0; i < N; ++i)                                 \
            woort_struct_set_real(s + 0, i, propv[i]);                 \
        woort_set_option_value(WOORT_RETURN_SLOT, s + 0);              \
        return woort_ret();                                            \
    }                                                                  \
    return woort_ret_option_none();                                    \
}
// add_property 型：option<T> 默认值 → option<int>（新属性下标）
#define JE_WO_PROP_ADDER(FN, TYPE, PROPVAL)                            \
WOORT_API woort_api FN(void)                                           \
{                                                                      \
    woort_value s;                                                     \
    const void* pv = nullptr;                                          \
    PROPVAL(2)                                                         \
    int32_t index = 0;                                                 \
    if (!je_tilemap_add_property(woje_ts_doc(0), woort_string(1),      \
        TYPE, pv, &index))                                             \
        return woort_ret_option_none();                                \
    return woort_ret_option_int(index);                                \
}
// find_cells 型：T 值（非 option）→ int（命中数）
#define JE_WO_PROP_FINDER(FN, TYPE, FINDARG)                           \
WOORT_API woort_api FN(void)                                           \
{                                                                      \
    FINDARG;                                                           \
    return woort_ret_int(je_tilemap_find_cells(woje_map_doc(0),        \
        (int32_t)woort_int(1), woort_string(2), TYPE, fvptr));         \
}
#define JE_WO_FINDARG_BOOL                                             \
    bool fv = woort_bool(3);                                           \
    const void* fvptr = &fv
#define JE_WO_FINDARG_INT                                              \
    int32_t fv = (int32_t)woort_int(3);                                \
    const void* fvptr = &fv
#define JE_WO_FINDARG_REAL                                             \
    float fv = (float)woort_real(3);                                   \
    const void* fvptr = &fv
#define JE_WO_FINDARG_STRING                                           \
    const char* fv = woort_string(3);                                  \
    const void* fvptr = fv
#define JE_WO_FINDARG_VEC2                                             \
    float fv[2] = { (float)woort_struct_get_real(3, 0),                \
                    (float)woort_struct_get_real(3, 1) };              \
    const void* fvptr = fv
#define JE_WO_FINDARG_VEC3                                             \
    float fv[3] = { (float)woort_struct_get_real(3, 0),                \
                    (float)woort_struct_get_real(3, 1),                \
                    (float)woort_struct_get_real(3, 2) };              \
    const void* fvptr = fv
#define JE_WO_FINDARG_VEC4                                             \
    float fv[4] = { (float)woort_struct_get_real(3, 0),                \
                    (float)woort_struct_get_real(3, 1),                \
                    (float)woort_struct_get_real(3, 2),                \
                    (float)woort_struct_get_real(3, 3) };              \
    const void* fvptr = fv

// ---- 地图侧：逐格覆盖 / 取值链解析 / 查找 ----
JE_WO_PROP_SETTER(wojeapi_tilemap_set_cell_property_bool,
    ([&](const void* pv)
        { return je_tilemap_set_cell_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_BOOL, pv); }),
    5, JE_WO_PROPVAL_BOOL)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_cell_property_int,
    ([&](const void* pv)
        { return je_tilemap_set_cell_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_INT, pv); }),
    5, JE_WO_PROPVAL_INT)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_cell_property_real,
    ([&](const void* pv)
        { return je_tilemap_set_cell_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_FLOAT, pv); }),
    5, JE_WO_PROPVAL_REAL)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_cell_property_string,
    ([&](const void* pv)
        { return je_tilemap_set_cell_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_STRING, pv); }),
    5, JE_WO_PROPVAL_STRING)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_cell_property_vec2,
    ([&](const void* pv)
        { return je_tilemap_set_cell_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC2, pv); }),
    5, JE_WO_PROPVAL_VEC2)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_cell_property_vec3,
    ([&](const void* pv)
        { return je_tilemap_set_cell_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC3, pv); }),
    5, JE_WO_PROPVAL_VEC3)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_cell_property_vec4,
    ([&](const void* pv)
        { return je_tilemap_set_cell_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC4, pv); }),
    5, JE_WO_PROPVAL_VEC4)

JE_WO_PROP_GETTER(wojeapi_tilemap_get_cell_property_bool,
    ([&](void* out)
        { return je_tilemap_get_cell_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_BOOL, out); }),
    bool, = false, woort_ret_option_bool)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_cell_property_int,
    ([&](void* out)
        { return je_tilemap_get_cell_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_INT, out); }),
    int32_t, = 0, woort_ret_option_int)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_cell_property_real,
    ([&](void* out)
        { return je_tilemap_get_cell_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_FLOAT, out); }),
    float, = 0.f, woort_ret_option_real)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_cell_property_string,
    ([&](void* out)
        { return je_tilemap_get_cell_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_STRING, out); }),
    const char*, = nullptr, woort_ret_option_string)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_cell_property_vec2,
    ([&](void* out)
        { return je_tilemap_get_cell_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC2, out); }),
    2)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_cell_property_vec3,
    ([&](void* out)
        { return je_tilemap_get_cell_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC3, out); }),
    3)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_cell_property_vec4,
    ([&](void* out)
        { return je_tilemap_get_cell_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC4, out); }),
    4)

JE_WO_PROP_GETTER(wojeapi_tilemap_get_property_bool,
    ([&](void* out)
        { return je_tilemap_get_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_BOOL, out); }),
    bool, = false, woort_ret_option_bool)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_property_int,
    ([&](void* out)
        { return je_tilemap_get_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_INT, out); }),
    int32_t, = 0, woort_ret_option_int)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_property_real,
    ([&](void* out)
        { return je_tilemap_get_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_FLOAT, out); }),
    float, = 0.f, woort_ret_option_real)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_property_string,
    ([&](void* out)
        { return je_tilemap_get_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_STRING, out); }),
    const char*, = nullptr, woort_ret_option_string)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_property_vec2,
    ([&](void* out)
        { return je_tilemap_get_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC2, out); }),
    2)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_property_vec3,
    ([&](void* out)
        { return je_tilemap_get_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC3, out); }),
    3)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_property_vec4,
    ([&](void* out)
        { return je_tilemap_get_property(woje_map_doc(0), (int32_t)woort_int(1),
            (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4),
            JE_PROPERTY_TYPE_VEC4, out); }),
    4)

JE_WO_PROP_FINDER(wojeapi_tilemap_find_cells_bool,
    JE_PROPERTY_TYPE_BOOL, JE_WO_FINDARG_BOOL)
JE_WO_PROP_FINDER(wojeapi_tilemap_find_cells_int,
    JE_PROPERTY_TYPE_INT, JE_WO_FINDARG_INT)
JE_WO_PROP_FINDER(wojeapi_tilemap_find_cells_real,
    JE_PROPERTY_TYPE_FLOAT, JE_WO_FINDARG_REAL)
JE_WO_PROP_FINDER(wojeapi_tilemap_find_cells_string,
    JE_PROPERTY_TYPE_STRING, JE_WO_FINDARG_STRING)
JE_WO_PROP_FINDER(wojeapi_tilemap_find_cells_vec2,
    JE_PROPERTY_TYPE_VEC2, JE_WO_FINDARG_VEC2)
JE_WO_PROP_FINDER(wojeapi_tilemap_find_cells_vec3,
    JE_PROPERTY_TYPE_VEC3, JE_WO_FINDARG_VEC3)
JE_WO_PROP_FINDER(wojeapi_tilemap_find_cells_vec4,
    JE_PROPERTY_TYPE_VEC4, JE_WO_FINDARG_VEC4)
WOORT_API woort_api wojeapi_tilemap_is_walkable(void)
{
    return woort_ret_int(je_tilemap_is_walkable(
        woje_map_doc(0), (int32_t)woort_int(1), (int32_t)woort_int(2)));
}
WOORT_API woort_api wojeapi_tilemap_is_walkable_on(void)
{
    return woort_ret_int(je_tilemap_is_walkable_on(
        woje_map_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3)));
}
WOORT_API woort_api wojeapi_tilemap_find_get(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    int32_t x = -1, y = -1;
    je_tilemap_find_get(woje_map_doc(0), (int32_t)woort_int(1), &x, &y);
    woort_set_struct(s + 0, 2);
    woort_struct_set_int(s + 0, 0, x);
    woort_struct_set_int(s + 0, 1, y);
    return woort_ret_value(s + 0);
}

// ---- 引用计数与相等性（gchandle 载荷即文档实例指针；同一文档按路径
// 共享唯一实例，指针相等必为同一文档，比较安全）----
WOORT_API woort_api wojeapi_tilemap_keep_map(void)
{
    return woort_ret_int(je_tilemap_keep_map(woje_map_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_unload_map(void)
{
    return woort_ret_int(je_tilemap_unload_map(woje_map_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_map_same(void)
{
    return woort_ret_bool(
        woje_map_doc(0) == woje_map_doc(1) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_keep_tileset(void)
{
    return woort_ret_int(je_tilemap_keep_tileset(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_unload_tileset(void)
{
    return woort_ret_int(je_tilemap_unload_tileset(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_tileset_same(void)
{
    return woort_ret_bool(
        woje_ts_doc(0) == woje_ts_doc(1) ? true : false);
}

WOORT_API woort_api wojeapi_tilemap_open_tileset(void)
{
    je_TilesetDocument* doc = je_tilemap_open_tileset(woort_string(0));
    if (doc == nullptr)
        return woort_ret_option_none();
    return woort_ret_option_gchandle(
        doc, WOORT_IGNORE, woje_tilemap_release_tileset, nullptr);
}
WOORT_API woort_api wojeapi_tilemap_create_tileset(void)
{
    je_TilesetDocument* doc = je_tilemap_create_tileset(woort_string(0));
    if (doc == nullptr)
        return woort_ret_option_none();
    return woort_ret_option_gchandle(
        doc, WOORT_IGNORE, woje_tilemap_release_tileset, nullptr);
}
WOORT_API woort_api wojeapi_tilemap_save_tileset(void)
{
    return woort_ret_bool(je_tilemap_save_tileset(
        woje_ts_doc(0), woort_string(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_reload_tileset(void)
{
    je_tilemap_reload_tileset(woort_string(0));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_tileset_version(void)
{
    return woort_ret_int((woort_Int)je_tilemap_tileset_version(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_tileset_doc_path(void)
{
    return woort_ret_string(je_tilemap_tileset_doc_path(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_tileset_set_name(void)
{
    je_tilemap_tileset_set_name(woje_ts_doc(0), woort_string(1));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_tileset_name(void)
{
    return woort_ret_string(je_tilemap_tileset_name(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_add_source(void)
{
    int32_t index = 0;
    if (!je_tilemap_add_source(
        woje_ts_doc(0), woort_string(1), (int32_t)woort_int(2), &index))
        return woort_ret_option_none();
    return woort_ret_option_int(index);
}
WOORT_API woort_api wojeapi_tilemap_remove_source(void)
{
    return woort_ret_bool(je_tilemap_remove_source(
        woje_ts_doc(0), (int32_t)woort_int(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_source_count(void)
{
    return woort_ret_int(je_tilemap_source_count(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_source_info(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    const char* path = "";
    int32_t tpx = 0, xc = 0, yc = 0;
    je_tilemap_source_info(woje_ts_doc(0), (int32_t)woort_int(1),
        &path, &tpx, &xc, &yc);
    woort_set_struct(s + 0, 4);
    woort_struct_set_string(s + 0, 0, path);
    woort_struct_set_int(s + 0, 1, tpx);
    woort_struct_set_int(s + 0, 2, xc);
    woort_struct_set_int(s + 0, 3, yc);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_add_tile(void)
{
    je_TileId id = 0;
    if (!je_tilemap_add_tile(
        woje_ts_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3), (int32_t)woort_int(4), &id))
        return woort_ret_option_none();
    return woort_ret_option_int(id);
}
WOORT_API woort_api wojeapi_tilemap_remove_tile(void)
{
    return woort_ret_bool(je_tilemap_remove_tile(
        woje_ts_doc(0), (int32_t)woort_int(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_tile_count(void)
{
    return woort_ret_int(je_tilemap_tile_count(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_tile_info(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    int32_t src = 0, ix = 0, iy = 0, wk = 1;
    je_tilemap_tile_info(woje_ts_doc(0), (int32_t)woort_int(1),
        &src, &ix, &iy, &wk);
    woort_set_struct(s + 0, 4);
    woort_struct_set_int(s + 0, 0, src);
    woort_struct_set_int(s + 0, 1, ix);
    woort_struct_set_int(s + 0, 2, iy);
    woort_struct_set_int(s + 0, 3, wk);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_set_tile_walkable(void)
{
    je_tilemap_set_tile_walkable(woje_ts_doc(0), (int32_t)woort_int(1), (int32_t)woort_int(2));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_add_terrain(void)
{
    je_TerrainId id = 0;
    if (!je_tilemap_add_terrain(
        woje_ts_doc(0), woort_string(1), (je_TerrainKind)woort_int(2),
        (int32_t)woort_int(3), (int32_t)woort_int(4), (int32_t)woort_int(5),
        (int32_t)woort_int(6), &id))
        return woort_ret_option_none();
    return woort_ret_option_int(id);
}
WOORT_API woort_api wojeapi_tilemap_remove_terrain(void)
{
    return woort_ret_bool(je_tilemap_remove_terrain(
        woje_ts_doc(0), (int32_t)woort_int(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_terrain_count(void)
{
    return woort_ret_int(je_tilemap_terrain_count(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_terrain_info(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    const char* name = "";
    je_TerrainKind kind = JE_TERRAIN_KIND_SINGLE;
    int32_t src = 0, ix = 0, iy = 0, wk = 1, vn = 0;
    je_tilemap_terrain_info(woje_ts_doc(0), (int32_t)woort_int(1),
        &name, &kind, &src, &ix, &iy, &wk, &vn);
    woort_set_struct(s + 0, 7);
    woort_struct_set_string(s + 0, 0, name);
    woort_struct_set_int(s + 0, 1, (int32_t)kind);
    woort_struct_set_int(s + 0, 2, src);
    woort_struct_set_int(s + 0, 3, ix);
    woort_struct_set_int(s + 0, 4, iy);
    woort_struct_set_int(s + 0, 5, wk);
    woort_struct_set_int(s + 0, 6, vn);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_set_terrain(void)
{
    return woort_ret_bool(je_tilemap_set_terrain(
        woje_ts_doc(0), (int32_t)woort_int(1), woort_string(2),
        (je_TerrainKind)woort_int(3), (int32_t)woort_int(4),
        (int32_t)woort_int(5), (int32_t)woort_int(6), (int32_t)woort_int(7)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_terrain_clear_variants(void)
{
    je_tilemap_terrain_clear_variants(woje_ts_doc(0), (int32_t)woort_int(1));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_terrain_add_variant(void)
{
    return woort_ret_bool(je_tilemap_terrain_add_variant(
        woje_ts_doc(0), (int32_t)woort_int(1), (int32_t)woort_int(2)) ? true : false);
}
// ---- 图集侧：schema 与默认值（按类型展开）----
JE_WO_PROP_ADDER(wojeapi_tilemap_add_property_bool,
    JE_PROPERTY_TYPE_BOOL, JE_WO_PROPVAL_BOOL)
JE_WO_PROP_ADDER(wojeapi_tilemap_add_property_int,
    JE_PROPERTY_TYPE_INT, JE_WO_PROPVAL_INT)
JE_WO_PROP_ADDER(wojeapi_tilemap_add_property_real,
    JE_PROPERTY_TYPE_FLOAT, JE_WO_PROPVAL_REAL)
JE_WO_PROP_ADDER(wojeapi_tilemap_add_property_string,
    JE_PROPERTY_TYPE_STRING, JE_WO_PROPVAL_STRING)
JE_WO_PROP_ADDER(wojeapi_tilemap_add_property_vec2,
    JE_PROPERTY_TYPE_VEC2, JE_WO_PROPVAL_VEC2)
JE_WO_PROP_ADDER(wojeapi_tilemap_add_property_vec3,
    JE_PROPERTY_TYPE_VEC3, JE_WO_PROPVAL_VEC3)
JE_WO_PROP_ADDER(wojeapi_tilemap_add_property_vec4,
    JE_PROPERTY_TYPE_VEC4, JE_WO_PROPVAL_VEC4)

WOORT_API woort_api wojeapi_tilemap_remove_property(void)
{
    return woort_ret_bool(je_tilemap_remove_property(
        woje_ts_doc(0), woort_string(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_property_count(void)
{
    return woort_ret_int(je_tilemap_property_count(woje_ts_doc(0)));
}
WOORT_API woort_api wojeapi_tilemap_property_info(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    const char* name = "";
    je_PropertyType type = JE_PROPERTY_TYPE_BOOL;
    je_tilemap_property_info(woje_ts_doc(0), (int32_t)woort_int(1),
        &name, &type);
    woort_set_struct(s + 0, 2);
    woort_struct_set_string(s + 0, 0, name);
    woort_struct_set_int(s + 0, 1, (int32_t)type);
    return woort_ret_value(s + 0);
}

JE_WO_PROP_GETTER(wojeapi_tilemap_property_default_bool,
    ([&](void* out)
        { return je_tilemap_property_default(woje_ts_doc(0), woort_string(1),
            JE_PROPERTY_TYPE_BOOL, out); }),
    bool, = false, woort_ret_option_bool)
JE_WO_PROP_GETTER(wojeapi_tilemap_property_default_int,
    ([&](void* out)
        { return je_tilemap_property_default(woje_ts_doc(0), woort_string(1),
            JE_PROPERTY_TYPE_INT, out); }),
    int32_t, = 0, woort_ret_option_int)
JE_WO_PROP_GETTER(wojeapi_tilemap_property_default_real,
    ([&](void* out)
        { return je_tilemap_property_default(woje_ts_doc(0), woort_string(1),
            JE_PROPERTY_TYPE_FLOAT, out); }),
    float, = 0.f, woort_ret_option_real)
JE_WO_PROP_GETTER(wojeapi_tilemap_property_default_string,
    ([&](void* out)
        { return je_tilemap_property_default(woje_ts_doc(0), woort_string(1),
            JE_PROPERTY_TYPE_STRING, out); }),
    const char*, = nullptr, woort_ret_option_string)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_property_default_vec2,
    ([&](void* out)
        { return je_tilemap_property_default(woje_ts_doc(0), woort_string(1),
            JE_PROPERTY_TYPE_VEC2, out); }),
    2)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_property_default_vec3,
    ([&](void* out)
        { return je_tilemap_property_default(woje_ts_doc(0), woort_string(1),
            JE_PROPERTY_TYPE_VEC3, out); }),
    3)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_property_default_vec4,
    ([&](void* out)
        { return je_tilemap_property_default(woje_ts_doc(0), woort_string(1),
            JE_PROPERTY_TYPE_VEC4, out); }),
    4)

// ---- 瓦片默认值 ----
JE_WO_PROP_SETTER(wojeapi_tilemap_set_tile_property_bool,
    ([&](const void* pv)
        { return je_tilemap_set_tile_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_BOOL, pv); }),
    3, JE_WO_PROPVAL_BOOL)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_tile_property_int,
    ([&](const void* pv)
        { return je_tilemap_set_tile_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_INT, pv); }),
    3, JE_WO_PROPVAL_INT)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_tile_property_real,
    ([&](const void* pv)
        { return je_tilemap_set_tile_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_FLOAT, pv); }),
    3, JE_WO_PROPVAL_REAL)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_tile_property_string,
    ([&](const void* pv)
        { return je_tilemap_set_tile_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_STRING, pv); }),
    3, JE_WO_PROPVAL_STRING)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_tile_property_vec2,
    ([&](const void* pv)
        { return je_tilemap_set_tile_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC2, pv); }),
    3, JE_WO_PROPVAL_VEC2)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_tile_property_vec3,
    ([&](const void* pv)
        { return je_tilemap_set_tile_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC3, pv); }),
    3, JE_WO_PROPVAL_VEC3)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_tile_property_vec4,
    ([&](const void* pv)
        { return je_tilemap_set_tile_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC4, pv); }),
    3, JE_WO_PROPVAL_VEC4)

JE_WO_PROP_GETTER(wojeapi_tilemap_get_tile_property_bool,
    ([&](void* out)
        { return je_tilemap_get_tile_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_BOOL, out); }),
    bool, = false, woort_ret_option_bool)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_tile_property_int,
    ([&](void* out)
        { return je_tilemap_get_tile_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_INT, out); }),
    int32_t, = 0, woort_ret_option_int)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_tile_property_real,
    ([&](void* out)
        { return je_tilemap_get_tile_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_FLOAT, out); }),
    float, = 0.f, woort_ret_option_real)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_tile_property_string,
    ([&](void* out)
        { return je_tilemap_get_tile_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_STRING, out); }),
    const char*, = nullptr, woort_ret_option_string)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_tile_property_vec2,
    ([&](void* out)
        { return je_tilemap_get_tile_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC2, out); }),
    2)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_tile_property_vec3,
    ([&](void* out)
        { return je_tilemap_get_tile_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC3, out); }),
    3)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_tile_property_vec4,
    ([&](void* out)
        { return je_tilemap_get_tile_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC4, out); }),
    4)

// ---- 地形默认值 ----
JE_WO_PROP_SETTER(wojeapi_tilemap_set_terrain_property_bool,
    ([&](const void* pv)
        { return je_tilemap_set_terrain_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_BOOL, pv); }),
    3, JE_WO_PROPVAL_BOOL)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_terrain_property_int,
    ([&](const void* pv)
        { return je_tilemap_set_terrain_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_INT, pv); }),
    3, JE_WO_PROPVAL_INT)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_terrain_property_real,
    ([&](const void* pv)
        { return je_tilemap_set_terrain_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_FLOAT, pv); }),
    3, JE_WO_PROPVAL_REAL)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_terrain_property_string,
    ([&](const void* pv)
        { return je_tilemap_set_terrain_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_STRING, pv); }),
    3, JE_WO_PROPVAL_STRING)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_terrain_property_vec2,
    ([&](const void* pv)
        { return je_tilemap_set_terrain_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC2, pv); }),
    3, JE_WO_PROPVAL_VEC2)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_terrain_property_vec3,
    ([&](const void* pv)
        { return je_tilemap_set_terrain_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC3, pv); }),
    3, JE_WO_PROPVAL_VEC3)
JE_WO_PROP_SETTER(wojeapi_tilemap_set_terrain_property_vec4,
    ([&](const void* pv)
        { return je_tilemap_set_terrain_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC4, pv); }),
    3, JE_WO_PROPVAL_VEC4)

JE_WO_PROP_GETTER(wojeapi_tilemap_get_terrain_property_bool,
    ([&](void* out)
        { return je_tilemap_get_terrain_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_BOOL, out); }),
    bool, = false, woort_ret_option_bool)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_terrain_property_int,
    ([&](void* out)
        { return je_tilemap_get_terrain_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_INT, out); }),
    int32_t, = 0, woort_ret_option_int)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_terrain_property_real,
    ([&](void* out)
        { return je_tilemap_get_terrain_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_FLOAT, out); }),
    float, = 0.f, woort_ret_option_real)
JE_WO_PROP_GETTER(wojeapi_tilemap_get_terrain_property_string,
    ([&](void* out)
        { return je_tilemap_get_terrain_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_STRING, out); }),
    const char*, = nullptr, woort_ret_option_string)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_terrain_property_vec2,
    ([&](void* out)
        { return je_tilemap_get_terrain_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC2, out); }),
    2)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_terrain_property_vec3,
    ([&](void* out)
        { return je_tilemap_get_terrain_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC3, out); }),
    3)
JE_WO_PROP_VECGETTER(wojeapi_tilemap_get_terrain_property_vec4,
    ([&](void* out)
        { return je_tilemap_get_terrain_property(woje_ts_doc(0), (int32_t)woort_int(1),
            woort_string(2), JE_PROPERTY_TYPE_VEC4, out); }),
    4)
WOORT_API woort_api wojeapi_tilemap_cell_quad_count(void)
{
    return woort_ret_int(je_tilemap_cell_quad_count(
        woje_map_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3)));
}
WOORT_API woort_api wojeapi_tilemap_cell_quad(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    const char* path = "";
    float u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f;
    int32_t qx = 0, qy = 0, qcols = 1;
    if (je_tilemap_cell_quad(
        woje_map_doc(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3), (int32_t)woort_int(4),
        &path, &u0, &v0, &u1, &v1, &qx, &qy, &qcols))
    {
        woort_set_struct(s + 0, 8);
        woort_struct_set_string(s + 0, 0, path);
        woort_struct_set_real(s + 0, 1, u0);
        woort_struct_set_real(s + 0, 2, v0);
        woort_struct_set_real(s + 0, 3, u1);
        woort_struct_set_real(s + 0, 4, v1);
        woort_struct_set_int(s + 0, 5, qx);
        woort_struct_set_int(s + 0, 6, qy);
        woort_struct_set_int(s + 0, 7, qcols);
        // 载荷为元组：经 set_option_value 包装进 option 返回
        woort_set_option_value(WOORT_RETURN_SLOT, s + 0);
        return woort_ret();
    }
    return woort_ret_option_none();
}
