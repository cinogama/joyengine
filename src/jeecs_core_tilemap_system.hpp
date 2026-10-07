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

/*
TilemapSystem [系统] 与 je_tilemap_* C API 实现
瓦片地图核心的渲染侧：把地图文档实例化为"图层 × 分块 × 图集源"的
渲染子实体。文档模型、序列化与自动图块解析见
jeecs_core_tilemap_model.hpp；本文件只做三件事：

  * TilemapSystem —— Update 相位把文档变化翻译成渲染分片的增删与
    顶点重写（渲染之前完成数据构建）；
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
        static constexpr size_t FLOATS_PER_VERTEX = 8;   // pos3 + uv2 + normal3
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
        // 每个地图根实体的分片表 + 上次网格扫描结果（版本戳命中时免扫）
        struct root_state
        {
            std::map<part_key, part_entry> parts;
            std::set<part_key> needed;
            uint64_t needed_stamp = 0;
            bool needed_valid = false;
        };

        std::unordered_map<uint32_t, root_state> _m_roots;
        std::vector<float> _m_staging;

        TilemapSystem(game_world w)
            : game_system(w)
        {
            _m_staging.assign(CHUNK * CHUNK * FLOATS_PER_CELL, 0.f);
        }

        // ---- 共享资源 ----

        // 共享的 Forward2D 着色器（共享资源缓存，路径加载）
        static std::optional<basic::resource<graphic::shader>> shared_tile_shader()
        {
            static std::optional<basic::resource<graphic::shader>> cached;
            static bool tried = false;
            if (!tried)
            {
                tried = true;
                cached = graphic::shader::load(nullptr, "!/builtin/shader/Forward2D.shader");
                if (!cached.has_value())
                    debug::logerr("Tilemap: unable to load builtin shader 'Forward2D.shader'.");
            }
            return cached;
        }

        // 根实体 Renderer::Shaders 指示的瓦片着色器（未挂/为空 → 无值）
        static std::optional<basic::resource<graphic::shader>> root_shader(
            const game_entity& root)
        {
            auto* comp = root.get_component<Renderer::Shaders>();
            if (comp == nullptr || comp->shaders.empty())
                return std::nullopt;
            return comp->shaders.front();
        }

        // 分片着色器与期望一致则不动，否则重写（资源句柄不同即更新）
        static void refresh_part_shader(const game_entity& root, const game_entity& part)
        {
            auto* comp = part.get_component<Renderer::Shaders>();
            if (comp == nullptr)
                return;
            auto desired = root_shader(root);
            if (!desired.has_value())
                desired = shared_tile_shader();
            if (!desired.has_value())
                return;
            const auto& want = desired.value();
            if (comp->shaders.size() == 1
                && comp->shaders.front().get() == want.get())
                return;
            comp->shaders.clear();
            comp->shaders.push_back(want);
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

        // 写一个四边形（x0/y0 左下，x1/y1 右上；v 引擎空间向上增长）
        static void write_quad(float* v,
            float x0, float y0, float x1, float y1,
            float u0, float v0, float u1, float v1)
        {
            // 顶点序：0=左下 1=右下 2=左上 3=右上（索引 0,1,2 2,1,3）
            float* p = v;
            *p++ = x0; *p++ = y0; *p++ = 0.f; *p++ = u0; *p++ = v0; *p++ = 0.f; *p++ = 0.f; *p++ = -1.f;
            *p++ = x1; *p++ = y0; *p++ = 0.f; *p++ = u1; *p++ = v0; *p++ = 0.f; *p++ = 0.f; *p++ = -1.f;
            *p++ = x0; *p++ = y1; *p++ = 0.f; *p++ = u0; *p++ = v1; *p++ = 0.f; *p++ = 0.f; *p++ = -1.f;
            *p++ = x1; *p++ = y1; *p++ = 0.f; *p++ = u1; *p++ = v1; *p++ = 0.f; *p++ = 0.f; *p++ = -1.f;
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
                write_quad(dst + q * 4 * FLOATS_PER_VERTEX, x0, ytop, x0, ytop, 0.f, 0.f, 0.f, 0.f);
            for (int32_t i = 0; i < count; ++i)
            {
                const auto& q = quads[i];
                if (q.corner < 0)
                {
                    write_quad(dst, x0, ybot, x1, ytop, q.u0, q.v0, q.u1, q.v1);
                }
                else
                {
                    // 四象限槽位：0=LU 1=RU 2=LD 3=RD
                    float cx0, cy0, cx1, cy1;
                    switch (q.corner)
                    {
                    case 0: cx0 = x0; cy0 = ym;  cx1 = xm; cy1 = ytop; break;
                    case 1: cx0 = xm;  cy0 = ym;  cx1 = x1; cy1 = ytop; break;
                    case 2: cx0 = x0;  cy0 = ybot; cx1 = xm; cy1 = ym;  break;
                    default: cx0 = xm;  cy0 = ybot; cx1 = x1; cy1 = ym;  break;
                    }
                    write_quad(dst + (size_t)q.corner * 4 * FLOATS_PER_VERTEX,
                        cx0, cy0, cx1, cy1, q.u0, q.v0, q.u1, q.v1);
                }
            }
        }

        // ---- 分片生命周期 ----

        game_entity create_part_entity(game_entity root, int32_t layer, float layer_z,
            const Tilemap::SourceTexture& src)
        {
            auto world = get_world();
            auto part = world.add_entity<
                Transform::LocalPosition, Transform::LocalRotation, Transform::LocalScale,
                Transform::LocalToParent, Transform::Translation,
                Renderer::Shape, Renderer::Shaders, Renderer::Textures,
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
            part.get_component<Renderer::Rendqueue>()->rend_queue = layer;

            // 比例尺：根实体的缩放即整图比例尺（变换系统的缩放为纯局部
            // 语义、不沿父链复合，故由本系统显式同步到各分片）
            if (auto* root_scale = root.get_component<Transform::LocalScale>())
                part.get_component<Transform::LocalScale>()->scale = root_scale->scale;

            // 着色器：根实体挂 Renderer::Shaders 时以其指示为准，
            // 否则用内置 Forward2D
            auto* shaders = part.get_component<Renderer::Shaders>();
            if (auto shad = root_shader(root); shad.has_value())
                shaders->shaders.push_back(shad.value());
            else if (auto def = shared_tile_shader())
                shaders->shaders.push_back(def.value());

            auto* textures = part.get_component<Renderer::Textures>();
            if (auto tex = graphic::texture::load(nullptr, src.path))
                textures->bind_texture(0, tex.value());

            auto* shape = part.get_component<Renderer::Shape>();
            auto created = graphic::vertex::create(
                jegl_vertex::TRIANGLES,
                _m_staging.data(),
                _m_staging.size() * sizeof(float),
                shared_indices(),
                {
                    { jegl_vertex::data_type::FLOAT32, 3 }, // pos
                    { jegl_vertex::data_type::FLOAT32, 2 }, // uv
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

            // 层 z 变化时同步分片位置；比例尺兜底写入
            //（常态由 sync_part_scale 每帧对齐，此处确保任何路径都不漏）
            if (auto* pos = e.get_component<Transform::LocalPosition>())
                pos->pos = math::vec3(0.f, 0.f, lz);
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
                        int32_t ts_idx = 0, local = 0;
                        Tilemap::decode_tile_value(v, &ts_idx, &local);
                        if (ts_idx < 0 || ts_idx >= (int32_t)doc.resolved.size()
                            || doc.resolved[ts_idx] == nullptr)
                            continue;
                        const auto* ts = doc.resolved[ts_idx].get();
                        int32_t src_idx = -1;
                        if (v > 0)
                        {
                            if (local > 0 && local < (int32_t)ts->tiles.size())
                                src_idx = ts->tiles[local].source;
                        }
                        else if (local > 0 && local < (int32_t)ts->terrains.size())
                            src_idx = ts->terrains[local].source;
                        if (src_idx < 0 || src_idx >= (int32_t)ts->sources.size())
                            continue;
                        needed.insert(part_key{
                            (int32_t)li, x / (int32_t)CHUNK, y / (int32_t)CHUNK,
                            ts_idx, src_idx });
                    }
                }
            }
        }

        void Update()
        {
            auto world = get_world();
            auto& docs = Tilemap::documents::inst();

            // 1. 收集地图根实体并解析文档
            struct root_info
            {
                je_GameEntity raw{};
                Tilemap::Map* comp = nullptr;
                Tilemap::MapDocument* doc = nullptr;
            };
            std::vector<root_info> roots;
            std::set<uint32_t> alive_root_ids;
            for (auto&& [e, map_comp] :
                query_entity<view typesof(Tilemap::Map&)>())
            {
                alive_root_ids.insert(e._m_raw._m_id);
                roots.push_back({ e._m_raw, &map_comp, nullptr });
            }
            for (auto& r : roots)
            {
                const std::string path =
                    r.comp != nullptr ? std::string(r.comp->map_path.c_str()) : std::string();
                r.doc = docs.open_map(path).second;
            }

            // 2. 回收孤儿分片与根簿记（所有者不存在或不再持有 Map 组件）
            std::vector<je_GameEntity> dead_parts;
            for (auto&& [e, part] :
                query_entity<view typesof(Tilemap::RenderPart&)>())
            {
                if (alive_root_ids.count(part.owner_id) == 0)
                    dead_parts.push_back(e._m_raw);
            }
            for (auto& raw : dead_parts)
                world.remove_entity(game_entity{ raw });
            for (auto it = _m_roots.begin(); it != _m_roots.end();)
            {
                if (alive_root_ids.count(it->first) == 0)
                    it = _m_roots.erase(it);
                else
                    ++it;
            }

            // 3. 逐根同步分片集合与版本
            for (auto& r : roots)
            {
                uint32_t root_id = r.raw._m_id;
                root_state& state = _m_roots[root_id];
                game_entity root{ r.raw };

                // 网格扫描只在版本戳变化时执行（map_stamp 已含图集实例身份，
                // 图集被重载/替换同样会触发重扫）
                uint64_t stamp = 0;
                if (r.doc != nullptr)
                    stamp = docs.map_stamp(*r.doc);
                if (!state.needed_valid || state.needed_stamp != stamp)
                {
                    state.needed.clear();
                    if (r.doc != nullptr)
                        scan_needed(*r.doc, state.needed);
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
                    auto fnd = state.parts.find(key);
                    if (fnd != state.parts.end())
                    {
                        if (r.doc != nullptr && fnd->second.built_stamp != stamp)
                        {
                            rebuild_part(*r.doc, root, key, fnd->second);
                            fnd->second.built_stamp = stamp;
                        }
                        // 着色器/比例尺跟随根实体（组件变化不计入地图
                        // 版本戳，故每帧对齐）
                        game_entity part{ fnd->second.entity };
                        refresh_part_shader(root, part);
                        sync_part_scale(root, part);
                        continue;
                    }

                    const Tilemap::SourceTexture* src_info = nullptr;
                    if (r.doc != nullptr
                        && key.tileset < (int32_t)r.doc->resolved.size()
                        && r.doc->resolved[key.tileset] != nullptr
                        && key.source < (int32_t)r.doc->resolved[key.tileset]->sources.size())
                        src_info = &r.doc->resolved[key.tileset]->sources[key.source];
                    if (src_info == nullptr)
                        continue;

                    game_entity part_entity = create_part_entity(
                        root, key.layer, r.doc->layers[key.layer].z, *src_info);
                    auto* tag = part_entity.get_component<Tilemap::RenderPart>();
                    tag->owner_id = root_id;
                    tag->layer = key.layer;
                    tag->chunk_x = key.chunk_x;
                    tag->chunk_y = key.chunk_y;
                    tag->source = key.tileset * 1024 + key.source;

                    part_entry entry{};
                    entry.entity = part_entity._m_raw;
                    if (r.doc != nullptr)
                    {
                        rebuild_part(*r.doc, root, key, entry);
                        entry.built_stamp = stamp;
                    }
                    state.parts[key] = entry;
                }
            }
        }
    };
}

// ======================================================================
// je_tilemap_* C API 实现（声明与 JE_API 导出标记统一见 include/jeecs.hpp）
// ======================================================================
jeecs::Tilemap::MapDocument* je_tilemap_map_doc(je_TilemapHandle id)
{
    return jeecs::Tilemap::documents::inst().map(id);
}
jeecs::Tilemap::TilesetDocument* je_tilemap_ts_doc(je_TilesetHandle id)
{
    return jeecs::Tilemap::documents::inst().tileset(id);
}

// ---------------- 地图文档 ----------------
je_TilemapHandle je_tilemap_open_map(const char* path)
{
    return jeecs::Tilemap::documents::inst().open_map(path ? path : "").first;
}
je_TilemapHandle je_tilemap_create_map(int32_t w, int32_t h, int32_t tile_px)
{
    if (w <= 0 || h <= 0 || tile_px <= 0 || w > 65536 || h > 65536)
        return 0;
    auto doc = std::make_shared<jeecs::Tilemap::MapDocument>();
    doc->width = w; doc->height = h; doc->tile_px = tile_px;
    jeecs::Tilemap::MapLayer layer;
    layer.name = "图层 1";
    layer.grid.assign((size_t)w * h, 0);
    doc->layers.push_back(std::move(layer));
    return jeecs::Tilemap::documents::inst().register_map(doc);
}
bool je_tilemap_save_map(je_TilemapHandle map, const char* path)
{
    return jeecs::Tilemap::documents::inst().save_map(map, path ? path : "");
}
void je_tilemap_reload_map(const char* path)
{
    jeecs::Tilemap::documents::inst().reload_map(path ? path : "");
}
void je_tilemap_map_size(je_TilemapHandle map, int32_t* w, int32_t* h, int32_t* tile_px, int32_t* layer_count)
{
    auto* doc = je_tilemap_map_doc(map);
    if (w != nullptr) *w = doc ? doc->width : 0;
    if (h != nullptr) *h = doc ? doc->height : 0;
    if (tile_px != nullptr) *tile_px = doc ? doc->tile_px : 0;
    if (layer_count != nullptr) *layer_count = doc ? (int32_t)doc->layers.size() : 0;
}
const char* je_tilemap_map_path(je_TilemapHandle map)
{
    auto* doc = je_tilemap_map_doc(map);
    return doc ? doc->path.c_str() : "";
}
uint64_t je_tilemap_map_version(je_TilemapHandle map)
{
    auto* doc = je_tilemap_map_doc(map);
    return doc ? doc->version : 0;
}
void je_tilemap_resize_map(je_TilemapHandle map, int32_t new_w, int32_t new_h)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || new_w <= 0 || new_h <= 0 || new_w > 65536 || new_h > 65536)
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
    std::map<std::pair<int32_t, int32_t>, std::map<std::string, std::string>> kept;
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
int32_t je_tilemap_add_layer(je_TilemapHandle map, const char* name)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr)
        return -1;
    jeecs::Tilemap::MapLayer layer;
    layer.name = name ? name : ("图层 " + std::to_string(doc->layers.size() + 1));
    layer.z = (float)doc->layers.size(); // 新层默认置于最上（沿 +z 错开）
    layer.grid.assign((size_t)doc->width * doc->height, 0);
    doc->layers.push_back(std::move(layer));
    ++doc->version;
    return (int32_t)doc->layers.size() - 1;
}
bool je_tilemap_remove_layer(je_TilemapHandle map, int32_t layer)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || layer < 0 || layer >= (int32_t)doc->layers.size())
        return false;
    doc->layers.erase(doc->layers.begin() + layer);
    // 图层下标变化：重建逐格属性键
    std::map<std::pair<int32_t, int32_t>, std::map<std::string, std::string>> kept;
    for (auto& [key, kvs] : doc->cell_properties)
    {
        if (key.first == layer) continue;
        kept[{ key.first > layer ? key.first - 1 : key.first, key.second }] = std::move(kvs);
    }
    doc->cell_properties = std::move(kept);
    ++doc->version;
    return true;
}
bool je_tilemap_move_layer(je_TilemapHandle map, int32_t layer, int32_t new_pos)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || layer < 0 || layer >= (int32_t)doc->layers.size())
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
    std::map<std::pair<int32_t, int32_t>, std::map<std::string, std::string>> kept;
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
void je_tilemap_get_layer(je_TilemapHandle map, int32_t layer,
    const char** name, int32_t* visible, int32_t* locked)
{
    auto* doc = je_tilemap_map_doc(map);
    const jeecs::Tilemap::MapLayer* l =
        doc != nullptr && layer >= 0 && layer < (int32_t)doc->layers.size()
        ? &doc->layers[layer] : nullptr;
    if (name != nullptr) *name = l ? l->name.c_str() : "";
    if (visible != nullptr) *visible = l ? (l->visible ? 1 : 0) : 0;
    if (locked != nullptr) *locked = l ? (l->locked ? 1 : 0) : 0;
}
void je_tilemap_set_layer(je_TilemapHandle map, int32_t layer,
    const char* name, int32_t visible, int32_t locked)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || layer < 0 || layer >= (int32_t)doc->layers.size())
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
float je_tilemap_layer_z(je_TilemapHandle map, int32_t layer)
{
    auto* doc = je_tilemap_map_doc(map);
    return doc != nullptr && layer >= 0 && layer < (int32_t)doc->layers.size()
        ? doc->layers[layer].z : 0.f;
}
void je_tilemap_set_layer_z(je_TilemapHandle map, int32_t layer, float z)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || layer < 0 || layer >= (int32_t)doc->layers.size())
        return;
    if (doc->layers[layer].z != z)
    {
        doc->layers[layer].z = z;
        ++doc->version;
    }
}

// ---------------- 瓦片读写 ----------------
int32_t je_tilemap_get_tile(je_TilemapHandle map, int32_t layer, int32_t x, int32_t y)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || !doc->in_bounds(layer, x, y))
        return 0;
    return doc->layers[layer].grid[y * doc->width + x];
}
void je_tilemap_set_tile(je_TilemapHandle map, int32_t layer, int32_t x, int32_t y, int32_t value)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || !doc->in_bounds(layer, x, y))
        return;
    int32_t& cell = doc->layers[layer].grid[y * doc->width + x];
    if (cell != value)
    {
        cell = value;
        ++doc->version;
    }
}
void je_tilemap_fill_tiles(je_TilemapHandle map, int32_t layer,
    int32_t x, int32_t y, int32_t w, int32_t h, int32_t value)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || layer < 0 || layer >= (int32_t)doc->layers.size())
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
int32_t je_tilemap_tileset_count(je_TilemapHandle map)
{
    auto* doc = je_tilemap_map_doc(map);
    return doc ? (int32_t)doc->tilesets.size() : 0;
}
int32_t je_tilemap_add_tileset(je_TilemapHandle map, const char* path)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || path == nullptr || path[0] == '\0')
        return -1;
    // 不要求图集此刻可解析（可先引用后保存图集）
    for (size_t i = 0; i < doc->tilesets.size(); ++i)
        if (doc->tilesets[i] == path)
            return (int32_t)i;
    doc->tilesets.push_back(path);
    doc->resolved.push_back(nullptr);
    ++doc->version;
    return (int32_t)doc->tilesets.size() - 1;
}
bool je_tilemap_remove_tileset(je_TilemapHandle map, int32_t idx)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || idx < 0 || idx >= (int32_t)doc->tilesets.size())
        return false;
    doc->tilesets.erase(doc->tilesets.begin() + idx);
    doc->resolved.erase(doc->resolved.begin() + idx);
    // 清除引用该图集的所有格子（编码随之失效）
    for (auto& layer : doc->layers)
        for (auto& v : layer.grid)
        {
            if (v == 0) continue;
            int32_t ts_idx = 0, local = 0;
            jeecs::Tilemap::decode_tile_value(v, &ts_idx, &local);
            if (ts_idx == idx) v = 0;
            else if (ts_idx > idx)
                v = v < 0
                ? -jeecs::Tilemap::encode_terrain_value(ts_idx - 1, local)
                : jeecs::Tilemap::encode_tile_value(ts_idx - 1, local);
        }
    ++doc->version;
    return true;
}
const char* je_tilemap_tileset_path(je_TilemapHandle map, int32_t idx)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || idx < 0 || idx >= (int32_t)doc->tilesets.size())
        return "";
    return doc->tilesets[idx].c_str();
}

// ---------------- 属性与查询 ----------------
void je_tilemap_set_cell_property(je_TilemapHandle map, int32_t layer,
    int32_t x, int32_t y, const char* name, const char* value)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || !doc->in_bounds(layer, x, y) || name == nullptr)
        return;
    auto key = std::make_pair(layer, x + y * doc->width);
    if (value == nullptr)
    {
        auto fnd = doc->cell_properties.find(key);
        if (fnd != doc->cell_properties.end())
        {
            fnd->second.erase(name);
            if (fnd->second.empty())
                doc->cell_properties.erase(fnd);
            ++doc->version;
        }
        return;
    }
    doc->cell_properties[key][name] = value;
    ++doc->version;
}
const char* je_tilemap_get_cell_property(je_TilemapHandle map, int32_t layer,
    int32_t x, int32_t y, const char* name)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || !doc->in_bounds(layer, x, y) || name == nullptr)
        return nullptr;
    auto fnd = doc->cell_properties.find({ layer, x + y * doc->width });
    if (fnd == doc->cell_properties.end())
        return nullptr;
    auto vit = fnd->second.find(name);
    return vit == fnd->second.end() ? nullptr : vit->second.c_str();
}
const char* je_tilemap_get_property(je_TilemapHandle map, int32_t layer,
    int32_t x, int32_t y, const char* name)
{
    auto* doc = je_tilemap_map_doc(map);
    if (name == nullptr)
        return nullptr;
    const std::string* r = jeecs::Tilemap::resolve_property(doc, layer, x, y, name);
    return r == nullptr ? nullptr : r->c_str();
}
int32_t je_tilemap_is_walkable(je_TilemapHandle map, int32_t x, int32_t y)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || !doc->in_bounds(0, x, y))
        return 0;   // 出界视为不可通行
    for (int32_t l = 0; l < (int32_t)doc->layers.size(); ++l)
        if (!jeecs::Tilemap::walkable_on(doc, l, x, y))
            return 0;
    return 1;
}
int32_t je_tilemap_is_walkable_on(je_TilemapHandle map, int32_t layer, int32_t x, int32_t y)
{
    auto* doc = je_tilemap_map_doc(map);
    return jeecs::Tilemap::walkable_on(doc, layer, x, y) ? 1 : 0;
}
int32_t je_tilemap_find_cells(je_TilemapHandle map, int32_t layer, const char* name, const char* value)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || name == nullptr || value == nullptr)
        return 0;
    auto& result = jeecs::Tilemap::documents::inst().find_result(doc);
    result.clear();
    std::set<int32_t> matched;
    auto try_layer = [&](int32_t l)
    {
        for (int32_t y = 0; y < doc->height; ++y)
            for (int32_t x = 0; x < doc->width; ++x)
            {
                if (matched.count(x + y * doc->width) != 0)
                    continue;
                bool hit = false;
                if (strcmp(name, "walkable") == 0)
                {
                    bool w = jeecs::Tilemap::walkable_on(doc, l, x, y);
                    hit = (strcmp(value, "true") == 0) == w;
                }
                else
                {
                    const std::string* r = jeecs::Tilemap::resolve_property(doc, l, x, y, name);
                    hit = r != nullptr && *r == value;
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
bool je_tilemap_find_get(je_TilemapHandle map, int32_t index, int32_t* x, int32_t* y)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr)
        return false;
    const auto& result = jeecs::Tilemap::documents::inst().find_result(doc);
    if (index < 0 || index >= (int32_t)result.size())
        return false;
    if (x != nullptr) *x = result[index].first;
    if (y != nullptr) *y = result[index].second;
    return true;
}

// ---------------- 图集文档 ----------------
je_TilesetHandle je_tilemap_open_tileset(const char* path)
{
    return jeecs::Tilemap::documents::inst().open_tileset(path ? path : "").first;
}
je_TilesetHandle je_tilemap_create_tileset(const char* name)
{
    auto doc = std::make_shared<jeecs::Tilemap::TilesetDocument>();
    doc->name = name ? name : "新建图集";
    doc->tiles.push_back({});
    doc->terrains.push_back({});
    return jeecs::Tilemap::documents::inst().register_tileset(doc);
}
bool je_tilemap_save_tileset(je_TilesetHandle tileset, const char* path)
{
    return jeecs::Tilemap::documents::inst().save_tileset(tileset, path ? path : "");
}
void je_tilemap_reload_tileset(const char* path)
{
    jeecs::Tilemap::documents::inst().reload_tileset(path ? path : "");
}
uint64_t je_tilemap_tileset_version(je_TilesetHandle tileset)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    return doc ? doc->version : 0;
}
const char* je_tilemap_tileset_doc_path(je_TilesetHandle tileset)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    return doc ? doc->path.c_str() : "";
}
void je_tilemap_tileset_set_name(je_TilesetHandle tileset, const char* name)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc != nullptr && name != nullptr)
    {
        doc->name = name;
        ++doc->version;
    }
}
const char* je_tilemap_tileset_name(je_TilesetHandle tileset)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    return doc ? doc->name.c_str() : "";
}

// ---------------- 图集源纹理 ----------------
int32_t je_tilemap_add_source(je_TilesetHandle tileset, const char* texture_path, int32_t tile_px)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || texture_path == nullptr || texture_path[0] == '\0'
        || tile_px <= 0 || tile_px > 4096)
        return -1;
    // 复用已有的同路径源
    for (size_t i = 0; i < doc->sources.size(); ++i)
        if (doc->sources[i].path == texture_path)
            return (int32_t)i;
    // 读取纹理尺寸以推导网格数
    int32_t tw = 0, th = 0;
    if (auto tex = jeecs::graphic::texture::load(nullptr, texture_path))
    {
        tw = (int32_t)tex->get()->resource()->m_width;
        th = (int32_t)tex->get()->resource()->m_height;
    }
    else
    {
        jeecs::debug::logerr("Tilemap: unable to load tileset texture '%s'.", texture_path);
        return -1;
    }
    if (tw < tile_px || th < tile_px)
        return -1;
    jeecs::Tilemap::SourceTexture src;
    src.path = texture_path;
    src.tile_px = tile_px;
    src.xcount = tw / tile_px;
    src.ycount = th / tile_px;
    src.tex_w = tw;
    src.tex_h = th;
    doc->sources.push_back(std::move(src));
    ++doc->version;
    return (int32_t)doc->sources.size() - 1;
}
bool je_tilemap_remove_source(je_TilesetHandle tileset, int32_t source_idx)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || source_idx < 0 || source_idx >= (int32_t)doc->sources.size())
        return false;
    doc->sources.erase(doc->sources.begin() + source_idx);
    // 引用该源的瓦片/地形做墓碑（source=-1），其余重排 source 下标。
    // 与 remove_terrain 同理：不移动数组元素，地图按 id 引用永不漂移。
    for (auto& d : doc->tiles)
    {
        if (d.source == source_idx) { d.source = -1; continue; }
        if (d.source > source_idx) d.source -= 1;
    }
    for (auto& d : doc->terrains)
    {
        if (d.source == source_idx)
        {
            d.source = -1;
            d.kind = 255;
            d.name.clear();
            d.ix = 0; d.iy = 0;
            d.variant_tiles.clear();
            d.properties.clear();
            continue;
        }
        if (d.source > source_idx) d.source -= 1;
    }
    doc->rebuild_flat_index();
    ++doc->version;
    return true;
}
int32_t je_tilemap_source_count(je_TilesetHandle tileset)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    return doc ? (int32_t)doc->sources.size() : 0;
}
bool je_tilemap_source_info(je_TilesetHandle tileset, int32_t source_idx,
    const char** texture_path, int32_t* tile_px, int32_t* xcount, int32_t* ycount)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || source_idx < 0 || source_idx >= (int32_t)doc->sources.size())
        return false;
    const auto& s = doc->sources[source_idx];
    if (texture_path != nullptr) *texture_path = s.path.c_str();
    if (tile_px != nullptr) *tile_px = s.tile_px;
    if (xcount != nullptr) *xcount = s.xcount;
    if (ycount != nullptr) *ycount = s.ycount;
    return true;
}

// ---------------- 普通瓦片表 ----------------
int32_t je_tilemap_add_tile(je_TilesetHandle tileset, int32_t source_idx, int32_t ix, int32_t iy, int32_t walkable)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || source_idx < 0 || source_idx >= (int32_t)doc->sources.size()
        || ix < 0 || iy < 0
        || ix >= doc->sources[source_idx].xcount || iy >= doc->sources[source_idx].ycount)
        return 0;
    // 复用已有同位置瓦片
    for (size_t i = 1; i < doc->tiles.size(); ++i)
        if (doc->tiles[i].source == source_idx
            && doc->tiles[i].ix == ix && doc->tiles[i].iy == iy)
            return (int32_t)i;
    jeecs::Tilemap::TileDef t;
    t.source = source_idx; t.ix = ix; t.iy = iy;
    t.walkable = walkable != 0;
    doc->tiles.push_back(t);
    doc->rebuild_flat_index();
    ++doc->version;
    return (int32_t)doc->tiles.size() - 1;
}
bool je_tilemap_remove_tile(je_TilesetHandle tileset, int32_t tile_id)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || tile_id <= 0 || tile_id >= (int32_t)doc->tiles.size())
        return false;
    // 墓碑删除（同 remove_terrain）：地图按 id 引用瓦片，不移动数组，
    // id 永不漂移，跨会话安全。
    auto& t = doc->tiles[tile_id];
    if (t.source < 0)
        return false;
    t.source = -1;
    t.ix = 0; t.iy = 0;
    doc->tile_properties.erase(tile_id);
    doc->rebuild_flat_index();
    ++doc->version;
    return true;
}
int32_t je_tilemap_tile_count(je_TilesetHandle tileset)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    return doc ? (int32_t)doc->tiles.size() - 1 : 0;
}
bool je_tilemap_tile_info(je_TilesetHandle tileset, int32_t tile_id,
    int32_t* source_idx, int32_t* ix, int32_t* iy, int32_t* walkable)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || tile_id <= 0 || tile_id >= (int32_t)doc->tiles.size())
        return false;
    const auto& t = doc->tiles[tile_id];
    if (source_idx != nullptr) *source_idx = t.source;
    if (ix != nullptr) *ix = t.ix;
    if (iy != nullptr) *iy = t.iy;
    if (walkable != nullptr) *walkable = t.walkable ? 1 : 0;
    return true;
}
void je_tilemap_set_tile_walkable(je_TilesetHandle tileset, int32_t tile_id, int32_t walkable)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || tile_id <= 0 || tile_id >= (int32_t)doc->tiles.size())
        return;
    doc->tiles[tile_id].walkable = walkable != 0;
    ++doc->version;
}

// ---------------- 自动图块地形 ----------------
int32_t je_tilemap_add_terrain(je_TilesetHandle tileset, const char* name, int32_t kind,
    int32_t source_idx, int32_t ix, int32_t iy, int32_t walkable)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || name == nullptr || kind < 0 || kind > 3
        || source_idx < 0 || source_idx >= (int32_t)doc->sources.size()
        || ix < 0 || iy < 0
        || ix >= doc->sources[source_idx].xcount || iy >= doc->sources[source_idx].ycount)
        return 0;
    jeecs::Tilemap::TerrainDef t;
    t.name = name;
    t.kind = kind;
    t.source = source_idx;
    t.ix = ix; t.iy = iy;
    t.walkable = walkable != 0;
    doc->terrains.push_back(std::move(t));
    ++doc->version;
    return (int32_t)doc->terrains.size() - 1;
}
bool je_tilemap_remove_terrain(je_TilesetHandle tileset, int32_t terrain_id)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size())
        return false;
    // 墓碑删除：只标记不移动数组。地图格子按 id 引用地形，若抹除元素，
    // 其后所有地形 id 前移，已保存地图（可能在其他会话中绘制）将整体
    // 错位。标记为 source=-1（渲染/查询自然失效），id 永不复用。
    auto& t = doc->terrains[terrain_id];
    if (t.source < 0)
        return false;
    t.source = -1;
    t.kind = 255; // 已删除标记（u8 序列化往返安全）
    t.name.clear();
    t.ix = 0; t.iy = 0;
    t.variant_tiles.clear();
    t.properties.clear();
    ++doc->version;
    return true;
}
int32_t je_tilemap_terrain_count(je_TilesetHandle tileset)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    return doc ? (int32_t)doc->terrains.size() - 1 : 0;
}
bool je_tilemap_terrain_info(je_TilesetHandle tileset, int32_t terrain_id,
    const char** name, int32_t* kind, int32_t* source_idx, int32_t* ix, int32_t* iy,
    int32_t* walkable, int32_t* variant_count)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size())
        return false;
    const auto& t = doc->terrains[terrain_id];
    if (name != nullptr) *name = t.name.c_str();
    if (kind != nullptr) *kind = t.kind;
    if (source_idx != nullptr) *source_idx = t.source;
    if (ix != nullptr) *ix = t.ix;
    if (iy != nullptr) *iy = t.iy;
    if (walkable != nullptr) *walkable = t.walkable ? 1 : 0;
    if (variant_count != nullptr) *variant_count = (int32_t)t.variant_tiles.size();
    return true;
}
bool je_tilemap_set_terrain(je_TilesetHandle tileset, int32_t terrain_id,
    const char* name, int32_t kind,
    int32_t source_idx, int32_t ix, int32_t iy, int32_t walkable)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size()
        || kind < 0 || kind > 3
        || source_idx < 0 || source_idx >= (int32_t)doc->sources.size()
        || ix < 0 || iy < 0
        || ix >= doc->sources[source_idx].xcount || iy >= doc->sources[source_idx].ycount)
        return false;
    // 墓碑槽位不可编辑（防止复活已删除 id 造成旧引用错位）
    if (doc->terrains[terrain_id].source < 0)
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
void je_tilemap_terrain_clear_variants(je_TilesetHandle tileset, int32_t terrain_id)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size())
        return;
    if (!doc->terrains[terrain_id].variant_tiles.empty())
    {
        doc->terrains[terrain_id].variant_tiles.clear();
        ++doc->version;
    }
}
bool je_tilemap_terrain_add_variant(je_TilesetHandle tileset, int32_t terrain_id, int32_t tile_id)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size()
        || tile_id <= 0 || tile_id >= (int32_t)doc->tiles.size())
        return false;
    doc->terrains[terrain_id].variant_tiles.push_back(tile_id);
    ++doc->version;
    return true;
}

// ---------------- 属性 schema 与默认值 ----------------
int32_t je_tilemap_add_property(je_TilesetHandle tileset, const char* name, const char* type, const char* default_value)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || name == nullptr || name[0] == '\0' || type == nullptr)
        return -1;
    for (const auto& pd : doc->properties)
        if (pd.name == name)
            return -1;
    doc->properties.push_back({ name, type, default_value ? default_value : "" });
    ++doc->version;
    return (int32_t)doc->properties.size() - 1;
}
bool je_tilemap_remove_property(je_TilesetHandle tileset, const char* name)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || name == nullptr)
        return false;
    for (size_t i = 0; i < doc->properties.size(); ++i)
        if (doc->properties[i].name == name)
        {
            doc->properties.erase(doc->properties.begin() + i);
            ++doc->version;
            return true;
        }
    return false;
}
int32_t je_tilemap_property_count(je_TilesetHandle tileset)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    return doc ? (int32_t)doc->properties.size() : 0;
}
bool je_tilemap_property_info(je_TilesetHandle tileset, int32_t index,
    const char** name, const char** type, const char** default_value)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || index < 0 || index >= (int32_t)doc->properties.size())
        return false;
    const auto& pd = doc->properties[index];
    if (name != nullptr) *name = pd.name.c_str();
    if (type != nullptr) *type = pd.type.c_str();
    if (default_value != nullptr) *default_value = pd.default_value.c_str();
    return true;
}
void je_tilemap_set_tile_property(je_TilesetHandle tileset, int32_t tile_id,
    const char* name, const char* value)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || tile_id <= 0 || tile_id >= (int32_t)doc->tiles.size()
        || name == nullptr)
        return;
    if (value == nullptr)
    {
        auto fnd = doc->tile_properties.find(tile_id);
        if (fnd != doc->tile_properties.end())
        {
            fnd->second.erase(name);
            if (fnd->second.empty())
                doc->tile_properties.erase(fnd);
            ++doc->version;
        }
        return;
    }
    doc->tile_properties[tile_id][name] = value;
    ++doc->version;
}
const char* je_tilemap_get_tile_property(je_TilesetHandle tileset, int32_t tile_id, const char* name)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || tile_id <= 0 || tile_id >= (int32_t)doc->tiles.size()
        || name == nullptr)
        return nullptr;
    auto fnd = doc->tile_properties.find(tile_id);
    if (fnd == doc->tile_properties.end())
        return nullptr;
    auto vit = fnd->second.find(name);
    return vit == fnd->second.end() ? nullptr : vit->second.c_str();
}
void je_tilemap_set_terrain_property(je_TilesetHandle tileset, int32_t terrain_id,
    const char* name, const char* value)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size()
        || name == nullptr)
        return;
    auto& props = doc->terrains[terrain_id].properties;
    if (value == nullptr)
    {
        if (props.erase(name) > 0)
            ++doc->version;
        return;
    }
    props[name] = value;
    ++doc->version;
}
const char* je_tilemap_get_terrain_property(je_TilesetHandle tileset, int32_t terrain_id, const char* name)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size()
        || name == nullptr)
        return nullptr;
    auto fnd = doc->terrains[terrain_id].properties.find(name);
    return fnd == doc->terrains[terrain_id].properties.end() ? nullptr : fnd->second.c_str();
}

// ======================================================================
// 单元格渲染信息（编辑器预览用）：resolve_cell_quads 的 C 边界映射
// ======================================================================
int32_t je_tilemap_cell_quad_count(je_TilemapHandle map, int32_t layer, int32_t x, int32_t y)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr)
        return 0;
    jeecs::Tilemap::cell_quad_info quads[4];
    return jeecs::Tilemap::resolve_cell_quads(*doc, layer, x, y, quads);
}
bool je_tilemap_cell_quad(je_TilemapHandle map, int32_t layer, int32_t x, int32_t y, int32_t idx,
    const char** texture_path,
    float* u0, float* v0, float* u1, float* v1,
    int32_t* qx, int32_t* qy, int32_t* qcols)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || idx < 0 || idx > 3)
        return false;
    jeecs::Tilemap::cell_quad_info quads[4];
    int32_t count = jeecs::Tilemap::resolve_cell_quads(*doc, layer, x, y, quads);
    if (idx >= count)
        return false;

    const auto& q = quads[idx];
    if (q.tileset_idx < 0 || q.tileset_idx >= (int32_t)doc->resolved.size()
        || doc->resolved[q.tileset_idx] == nullptr
        || q.source_idx < 0 || q.source_idx >= (int32_t)doc->resolved[q.tileset_idx]->sources.size())
        return false;

    if (texture_path != nullptr)
        *texture_path = doc->resolved[q.tileset_idx]->sources[q.source_idx].path.c_str();
    if (u0 != nullptr) *u0 = q.u0;
    if (v0 != nullptr) *v0 = q.v0;
    if (u1 != nullptr) *u1 = q.u1;
    if (v1 != nullptr) *v1 = q.v1;
    // 象限自左上角起于 qcols×qcols 网格：corner 0..3 -> (0,0)(1,0)(0,1)(1,1)
    if (qx != nullptr) *qx = q.corner < 0 ? 0 : (q.corner & 1);
    if (qy != nullptr) *qy = q.corner < 0 ? 0 : (q.corner >> 1);
    if (qcols != nullptr) *qcols = q.corner < 0 ? 1 : 2;
    return true;
}

// ======================================================================
// wojeapi_tilemap_*：Woolang 绑定（je/tilemap.wo 经 extern 声明引用）
// ======================================================================
WOORT_API woort_api wojeapi_tilemap_open_map(void)
{
    je_TilemapHandle map = je_tilemap_open_map(woort_string(0));
    if (map <= 0)
        return woort_ret_option_none();
    return woort_ret_option_pointer((void*)(intptr_t)map);
}
WOORT_API woort_api wojeapi_tilemap_create_map(void)
{
    je_TilemapHandle map = je_tilemap_create_map(
        (int32_t)woort_int(0), (int32_t)woort_int(1), (int32_t)woort_int(2));
    if (map <= 0)
        return woort_ret_option_none();
    return woort_ret_option_pointer((void*)(intptr_t)map);
}
WOORT_API woort_api wojeapi_tilemap_save_map(void)
{
    return woort_ret_bool(je_tilemap_save_map(
        (int32_t)woort_int(0), woort_string(1)) ? true : false);
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
    je_tilemap_map_size((int32_t)woort_int(0), &w, &h, &tpx, &lc);
    woort_set_struct(s + 0, 4);
    woort_struct_set_int(s + 0, 0, w);
    woort_struct_set_int(s + 0, 1, h);
    woort_struct_set_int(s + 0, 2, tpx);
    woort_struct_set_int(s + 0, 3, lc);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_map_path(void)
{
    return woort_ret_string(je_tilemap_map_path((int32_t)woort_int(0)));
}
WOORT_API woort_api wojeapi_tilemap_map_version(void)
{
    return woort_ret_int((woort_Int)je_tilemap_map_version((int32_t)woort_int(0)));
}
WOORT_API woort_api wojeapi_tilemap_resize_map(void)
{
    je_tilemap_resize_map((int32_t)woort_int(0), (int32_t)woort_int(1), (int32_t)woort_int(2));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_add_layer(void)
{
    return woort_ret_int(je_tilemap_add_layer((int32_t)woort_int(0), woort_string(1)));
}
WOORT_API woort_api wojeapi_tilemap_remove_layer(void)
{
    return woort_ret_bool(je_tilemap_remove_layer(
        (int32_t)woort_int(0), (int32_t)woort_int(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_move_layer(void)
{
    return woort_ret_bool(je_tilemap_move_layer(
        (int32_t)woort_int(0), (int32_t)woort_int(1), (int32_t)woort_int(2)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_get_layer(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    const char* name = "";
    int32_t visible = 0, locked = 0;
    je_tilemap_get_layer((int32_t)woort_int(0), (int32_t)woort_int(1),
        &name, &visible, &locked);
    woort_set_struct(s + 0, 3);
    woort_struct_set_string(s + 0, 0, name);
    woort_struct_set_int(s + 0, 1, visible);
    woort_struct_set_int(s + 0, 2, locked);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_set_layer(void)
{
    je_tilemap_set_layer((int32_t)woort_int(0), (int32_t)woort_int(1),
        woort_string(2), (int32_t)woort_int(3), (int32_t)woort_int(4));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_layer_z(void)
{
    return woort_ret_float(je_tilemap_layer_z(
        (int32_t)woort_int(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_set_layer_z(void)
{
    je_tilemap_set_layer_z((int32_t)woort_int(0), (int32_t)woort_int(1),
        woort_float(2));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_get_tile(void)
{
    return woort_ret_int(je_tilemap_get_tile(
        (int32_t)woort_int(0), (int32_t)woort_int(1), (int32_t)woort_int(2), (int32_t)woort_int(3)));
}
WOORT_API woort_api wojeapi_tilemap_set_tile(void)
{
    je_tilemap_set_tile((int32_t)woort_int(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3), (int32_t)woort_int(4));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_fill_tiles(void)
{
    je_tilemap_fill_tiles((int32_t)woort_int(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3),
        (int32_t)woort_int(4), (int32_t)woort_int(5), (int32_t)woort_int(6));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_tileset_count(void)
{
    return woort_ret_int(je_tilemap_tileset_count((int32_t)woort_int(0)));
}
WOORT_API woort_api wojeapi_tilemap_add_tileset(void)
{
    return woort_ret_int(je_tilemap_add_tileset((int32_t)woort_int(0), woort_string(1)));
}
WOORT_API woort_api wojeapi_tilemap_remove_tileset(void)
{
    return woort_ret_bool(je_tilemap_remove_tileset(
        (int32_t)woort_int(0), (int32_t)woort_int(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_tileset_path(void)
{
    return woort_ret_string(je_tilemap_tileset_path(
        (int32_t)woort_int(0), (int32_t)woort_int(1)));
}
WOORT_API woort_api wojeapi_tilemap_set_cell_property(void)
{
    // 第 5 参 value 为 option<string>：none 表示清除覆盖
    woort_value s;
    const char* value = nullptr;
    if (woort_push_reserve(1, &s) && woort_option_get(s, 5))
        value = woort_string(s);
    je_tilemap_set_cell_property((int32_t)woort_int(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4), value);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_get_cell_property(void)
{
    const char* r = je_tilemap_get_cell_property(
        (int32_t)woort_int(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4));
    if (r == nullptr)
        return woort_ret_option_none();
    return woort_ret_option_string(r);
}
WOORT_API woort_api wojeapi_tilemap_get_property(void)
{
    const char* r = je_tilemap_get_property(
        (int32_t)woort_int(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3), woort_string(4));
    if (r == nullptr)
        return woort_ret_option_none();
    return woort_ret_option_string(r);
}
WOORT_API woort_api wojeapi_tilemap_is_walkable(void)
{
    return woort_ret_int(je_tilemap_is_walkable(
        (int32_t)woort_int(0), (int32_t)woort_int(1), (int32_t)woort_int(2)));
}
WOORT_API woort_api wojeapi_tilemap_is_walkable_on(void)
{
    return woort_ret_int(je_tilemap_is_walkable_on(
        (int32_t)woort_int(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3)));
}
WOORT_API woort_api wojeapi_tilemap_find_cells(void)
{
    return woort_ret_int(je_tilemap_find_cells(
        (int32_t)woort_int(0), (int32_t)woort_int(1), woort_string(2), woort_string(3)));
}
WOORT_API woort_api wojeapi_tilemap_find_get(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    int32_t x = -1, y = -1;
    je_tilemap_find_get((int32_t)woort_int(0), (int32_t)woort_int(1), &x, &y);
    woort_set_struct(s + 0, 2);
    woort_struct_set_int(s + 0, 0, x);
    woort_struct_set_int(s + 0, 1, y);
    return woort_ret_value(s + 0);
}

WOORT_API woort_api wojeapi_tilemap_open_tileset(void)
{
    je_TilesetHandle tileset = je_tilemap_open_tileset(woort_string(0));
    if (tileset <= 0)
        return woort_ret_option_none();
    return woort_ret_option_pointer((void*)(intptr_t)tileset);
}
WOORT_API woort_api wojeapi_tilemap_create_tileset(void)
{
    je_TilesetHandle tileset = je_tilemap_create_tileset(woort_string(0));
    if (tileset <= 0)
        return woort_ret_option_none();
    return woort_ret_option_pointer((void*)(intptr_t)tileset);
}
WOORT_API woort_api wojeapi_tilemap_save_tileset(void)
{
    return woort_ret_bool(je_tilemap_save_tileset(
        (int32_t)woort_int(0), woort_string(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_reload_tileset(void)
{
    je_tilemap_reload_tileset(woort_string(0));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_tileset_version(void)
{
    return woort_ret_int((woort_Int)je_tilemap_tileset_version((int32_t)woort_int(0)));
}
WOORT_API woort_api wojeapi_tilemap_tileset_doc_path(void)
{
    return woort_ret_string(je_tilemap_tileset_doc_path((int32_t)woort_int(0)));
}
WOORT_API woort_api wojeapi_tilemap_tileset_set_name(void)
{
    je_tilemap_tileset_set_name((int32_t)woort_int(0), woort_string(1));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_tileset_name(void)
{
    return woort_ret_string(je_tilemap_tileset_name((int32_t)woort_int(0)));
}
WOORT_API woort_api wojeapi_tilemap_add_source(void)
{
    return woort_ret_int(je_tilemap_add_source(
        (int32_t)woort_int(0), woort_string(1), (int32_t)woort_int(2)));
}
WOORT_API woort_api wojeapi_tilemap_remove_source(void)
{
    return woort_ret_bool(je_tilemap_remove_source(
        (int32_t)woort_int(0), (int32_t)woort_int(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_source_count(void)
{
    return woort_ret_int(je_tilemap_source_count((int32_t)woort_int(0)));
}
WOORT_API woort_api wojeapi_tilemap_source_info(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    const char* path = "";
    int32_t tpx = 0, xc = 0, yc = 0;
    je_tilemap_source_info((int32_t)woort_int(0), (int32_t)woort_int(1),
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
    return woort_ret_int(je_tilemap_add_tile(
        (int32_t)woort_int(0), (int32_t)woort_int(1),
        (int32_t)woort_int(2), (int32_t)woort_int(3), (int32_t)woort_int(4)));
}
WOORT_API woort_api wojeapi_tilemap_remove_tile(void)
{
    return woort_ret_bool(je_tilemap_remove_tile(
        (int32_t)woort_int(0), (int32_t)woort_int(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_tile_count(void)
{
    return woort_ret_int(je_tilemap_tile_count((int32_t)woort_int(0)));
}
WOORT_API woort_api wojeapi_tilemap_tile_info(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    int32_t src = 0, ix = 0, iy = 0, wk = 1;
    je_tilemap_tile_info((int32_t)woort_int(0), (int32_t)woort_int(1),
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
    je_tilemap_set_tile_walkable((int32_t)woort_int(0), (int32_t)woort_int(1), (int32_t)woort_int(2));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_add_terrain(void)
{
    return woort_ret_int(je_tilemap_add_terrain(
        (int32_t)woort_int(0), woort_string(1), (int32_t)woort_int(2),
        (int32_t)woort_int(3), (int32_t)woort_int(4), (int32_t)woort_int(5), (int32_t)woort_int(6)));
}
WOORT_API woort_api wojeapi_tilemap_remove_terrain(void)
{
    return woort_ret_bool(je_tilemap_remove_terrain(
        (int32_t)woort_int(0), (int32_t)woort_int(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_terrain_count(void)
{
    return woort_ret_int(je_tilemap_terrain_count((int32_t)woort_int(0)));
}
WOORT_API woort_api wojeapi_tilemap_terrain_info(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    const char* name = "";
    int32_t kind = 0, src = 0, ix = 0, iy = 0, wk = 1, vn = 0;
    je_tilemap_terrain_info((int32_t)woort_int(0), (int32_t)woort_int(1),
        &name, &kind, &src, &ix, &iy, &wk, &vn);
    woort_set_struct(s + 0, 7);
    woort_struct_set_string(s + 0, 0, name);
    woort_struct_set_int(s + 0, 1, kind);
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
        (int32_t)woort_int(0), (int32_t)woort_int(1), woort_string(2),
        (int32_t)woort_int(3), (int32_t)woort_int(4),
        (int32_t)woort_int(5), (int32_t)woort_int(6), (int32_t)woort_int(7)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_terrain_clear_variants(void)
{
    je_tilemap_terrain_clear_variants((int32_t)woort_int(0), (int32_t)woort_int(1));
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_terrain_add_variant(void)
{
    return woort_ret_bool(je_tilemap_terrain_add_variant(
        (int32_t)woort_int(0), (int32_t)woort_int(1), (int32_t)woort_int(2)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_add_property(void)
{
    return woort_ret_int(je_tilemap_add_property(
        (int32_t)woort_int(0), woort_string(1), woort_string(2), woort_string(3)));
}
WOORT_API woort_api wojeapi_tilemap_remove_property(void)
{
    return woort_ret_bool(je_tilemap_remove_property(
        (int32_t)woort_int(0), woort_string(1)) ? true : false);
}
WOORT_API woort_api wojeapi_tilemap_property_count(void)
{
    return woort_ret_int(je_tilemap_property_count((int32_t)woort_int(0)));
}
WOORT_API woort_api wojeapi_tilemap_property_info(void)
{
    woort_value s;
    if (!woort_push_reserve(1, &s))
        return woort_ret_panic("Stack overflow.");
    const char* name = "", * type = "", * def = "";
    je_tilemap_property_info((int32_t)woort_int(0), (int32_t)woort_int(1),
        &name, &type, &def);
    woort_set_struct(s + 0, 3);
    woort_struct_set_string(s + 0, 0, name);
    woort_struct_set_string(s + 0, 1, type);
    woort_struct_set_string(s + 0, 2, def);
    return woort_ret_value(s + 0);
}
WOORT_API woort_api wojeapi_tilemap_set_tile_property(void)
{
    // 第 3 参 value 为 option<string>：none 表示清除
    woort_value s;
    const char* value = nullptr;
    if (woort_push_reserve(1, &s) && woort_option_get(s, 3))
        value = woort_string(s);
    je_tilemap_set_tile_property((int32_t)woort_int(0), (int32_t)woort_int(1),
        woort_string(2), value);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_get_tile_property(void)
{
    const char* r = je_tilemap_get_tile_property(
        (int32_t)woort_int(0), (int32_t)woort_int(1), woort_string(2));
    if (r == nullptr)
        return woort_ret_option_none();
    return woort_ret_option_string(r);
}
WOORT_API woort_api wojeapi_tilemap_set_terrain_property(void)
{
    // 第 3 参 value 为 option<string>：none 表示清除
    woort_value s;
    const char* value = nullptr;
    if (woort_push_reserve(1, &s) && woort_option_get(s, 3))
        value = woort_string(s);
    je_tilemap_set_terrain_property((int32_t)woort_int(0), (int32_t)woort_int(1),
        woort_string(2), value);
    return woort_ret_void();
}
WOORT_API woort_api wojeapi_tilemap_get_terrain_property(void)
{
    const char* r = je_tilemap_get_terrain_property(
        (int32_t)woort_int(0), (int32_t)woort_int(1), woort_string(2));
    if (r == nullptr)
        return woort_ret_option_none();
    return woort_ret_option_string(r);
}
WOORT_API woort_api wojeapi_tilemap_cell_quad_count(void)
{
    return woort_ret_int(je_tilemap_cell_quad_count(
        (int32_t)woort_int(0), (int32_t)woort_int(1),
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
        (int32_t)woort_int(0), (int32_t)woort_int(1),
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
