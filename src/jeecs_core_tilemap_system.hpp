#pragma once

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#   error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <string>
#include <memory>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

/*
TilemapSystem [系统] 与 je_tilemap_* C API 实现
瓦片地图核心：文档模型、.je4tilemap/.je4tileset 二进制读写、自动图块
（RPGMaker 式四象限 / blob47）、属性取值链、运行时查询，以及把地图
实例化为"图层 × 分块 × 图集"渲染子实体的系统。

工作方式：
  * 地图/图集文档进程内全局缓存（documents），按路径共享；编辑器经
    je_tilemap_* 修改文档后 version 自增，各世界的 TilemapSystem 在
    Update 相位比对版本并重建受影响的渲染分片 —— 未保存的编辑也会
    实时同步到所有世界实例（编辑器与世界共用同一份文档）。
  * 渲染分片（RenderPart）为地图根实体的子实体，每个分片持有
    32×32 格、单一图集纹理的动态顶点缓冲（创建时一次分配到容量上限、
    静态角点索引，之后整体重写并收缩激活索引数，与粒子系统同款）。
    分片实体带 Editor::Invisible：不显示于实体列表、不参与世界保存，
    加载/重载后由本系统自动重建。
  * 自动图块在网格中只存地形 id，邻接掩码与具体变体在构建顶点时
    动态解析，因此相邻地形变化时无需改写网格数据。
运行相位：Update（渲染之前完成数据构建）。
*/

namespace jeecs
{
    using namespace Renderer;
    using namespace Transform;

    namespace Tilemap
    {
        // ==================== 瓦片值编码 ====================
        // 0 = 空；v>0 普通瓦片；v<0 自动图块地形
        // |v| = (tileset_idx + 1) * TILE_ID_BASE + local_id
        constexpr int32_t TILE_ID_BASE = 0x100000;

        inline int32_t encode_tile_value(int32_t tileset_idx, int32_t local_id)
        {
            return (tileset_idx + 1) * TILE_ID_BASE + local_id;
        }
        inline int32_t encode_terrain_value(int32_t tileset_idx, int32_t local_id)
        {
            return -encode_tile_value(tileset_idx, local_id);
        }
        // 解码出 (tileset_idx, |v| 里的 local id)；不关心正负时可先取 abs
        inline bool decode_tile_value(int32_t v, int32_t* tileset_idx, int32_t* local_id)
        {
            if (v == 0)
                return false;
            int32_t a = v < 0 ? -v : v;
            *tileset_idx = a / TILE_ID_BASE - 1;
            *local_id = a % TILE_ID_BASE;
            return true;
        }

        // ==================== 文档数据模型 ====================
        struct SourceTexture
        {
            std::string path;   // 运行时路径（@/ 或 !/）
            int32_t tile_px = 32;
            int32_t xcount = 1, ycount = 1;
            int32_t tex_w = 0, tex_h = 0;   // 纹理像素尺寸（UV 计算用）
        };
        struct TileDef
        {
            int32_t source = 0; // 所属源纹理下标
            int32_t ix = 0, iy = 0; // 图集网格坐标（自左上角）
            bool walkable = true;
        };
        struct TerrainDef
        {
            std::string name;
            int32_t kind = 0;   // 0=单块 1=RPGMaker四象限 2=blob47
            int32_t source = 0;
            int32_t ix = 0, iy = 0; // 锚点图集网格坐标
            bool walkable = true;
            std::vector<int32_t> variant_tiles;             // blob47 显式变体表（普通瓦片 id）
            std::map<std::string, std::string> properties;  // 地形默认属性
        };
        struct PropertyDef
        {
            std::string name;
            std::string type;           // "bool"/"int"/"float"/"string"/"vec2"/"vec3"/"vec4"
            std::string default_value;
        };

        struct TilesetDocument
        {
            std::string name;
            std::string path;   // .je4tileset 运行时路径；未保存为空
            std::vector<SourceTexture> sources;
            std::vector<TileDef> tiles;        // tiles[0] 占位，瓦片 id = 下标（1 起）
            std::vector<TerrainDef> terrains;   // terrains[0] 占位，地形 id = 下标（1 起）
            std::vector<PropertyDef> properties;
            std::map<int32_t, std::map<std::string, std::string>> tile_properties;

            uint64_t version = 1;
            // (source<<32|flat_pos) -> tile id 的反查表，懒构建
            std::unordered_map<int64_t, int32_t> flat_tile_index;

            int32_t tile_at_flat(int32_t source_idx, int32_t flat) const
            {
                auto fnd = flat_tile_index.find(((int64_t)source_idx << 32) | (uint32_t)flat);
                return fnd == flat_tile_index.end() ? 0 : fnd->second;
            }
            void rebuild_flat_index()
            {
                flat_tile_index.clear();
                for (size_t i = 1; i < tiles.size(); ++i)
                {
                    const auto& t = tiles[i];
                    if (t.source < 0 || t.source >= (int32_t)sources.size())
                        continue;
                    flat_tile_index[((int64_t)t.source << 32)
                        | (uint32_t)(t.iy * sources[t.source].xcount + t.ix)] = (int32_t)i;
                }
            }
        };

        struct MapLayer
        {
            std::string name;
            bool visible = true;
            bool locked = false;    // 编辑器锁定标记（运行时忽略）
            std::vector<int32_t> grid;  // w*h 稠密网格，行优先、y 自上而下
        };
        struct MapDocument
        {
            std::string path;   // .je4tilemap 运行时路径；未保存为空
            int32_t width = 0, height = 0, tile_px = 32;
            std::vector<std::string> tilesets;  // 引用的图集路径
            std::vector<std::shared_ptr<TilesetDocument>> resolved;    // 与 tilesets 对应
            std::vector<MapLayer> layers;
            // 逐格属性覆盖：(layer, x + y*width) -> (name -> value)
            std::map<std::pair<int32_t, int32_t>, std::map<std::string, std::string>> cell_properties;

            uint64_t version = 1;
            std::vector<std::pair<int32_t, int32_t>> find_result; // find_cells 结果缓存

            bool in_bounds(int32_t layer, int32_t x, int32_t y) const
            {
                return layer >= 0 && layer < (int32_t)layers.size()
                    && x >= 0 && x < width && y >= 0 && y < height;
            }
        };

        // ==================== 二进制读写 ====================
        // 与 .je4animation 同风格的自有二进制格式（魔数 + 小端字段），
        // 由 C++ 侧统一读写，避免在内核引入 TOML 依赖。
        constexpr uint32_t TILESET_MAGIC = 0x4A545331; // "JTS1"
        constexpr uint32_t MAP_MAGIC = 0x4A544D31;     // "JTM1"

        struct bin_writer
        {
            std::vector<char> b;
            void u32(uint32_t v) { b.insert(b.end(), (char*)&v, (char*)&v + 4); }
            void i32(int32_t v) { u32((uint32_t)v); }
            void u8(uint8_t v) { b.push_back((char)v); }
            void str(const std::string& s)
            {
                u32((uint32_t)s.size());
                b.insert(b.end(), s.begin(), s.end());
            }
        };
        struct bin_reader
        {
            const char* p = nullptr;
            size_t n = 0, o = 0;
            bool ok = true;
            bool need(size_t c)
            {
                if (!ok || o + c > n) { ok = false; return false; }
                return true;
            }
            uint32_t u32()
            {
                if (!need(4)) return 0;
                uint32_t v; memcpy(&v, p + o, 4); o += 4; return v;
            }
            int32_t i32() { return (int32_t)u32(); }
            uint8_t u8()
            {
                if (!need(1)) return 0;
                return (uint8_t)p[o++];
            }
            std::string str()
            {
                uint32_t len = u32();
                if (!ok || len > 0x10000000 || !need(len)) { ok = false; return ""; }
                std::string s(p + o, len); o += len; return s;
            }
        };

        // @/!/ 前缀 -> 本地文件系统路径（保存用；读取走 jeecs_file_open 支持 fimg 包）
        inline std::string resolve_runtime_prefix(const std::string& path)
        {
            if (!path.empty() && path[0] == '@')
                return std::string(::jeecs_file_get_runtime_path()) + path.substr(1);
            if (!path.empty() && path[0] == '!')
                return std::string(::jeecs_file_get_host_path()) + path.substr(1);
            return path;
        }

        inline bool save_tileset_to(const TilesetDocument& ts, const std::string& path)
        {
            bin_writer w;
            w.u32(TILESET_MAGIC);
            w.str(ts.name);
            w.u32((uint32_t)ts.sources.size());
            for (const auto& s : ts.sources)
            {
                w.str(s.path); w.i32(s.tile_px); w.i32(s.xcount); w.i32(s.ycount);
                w.i32(s.tex_w); w.i32(s.tex_h);
            }
            w.u32((uint32_t)(ts.tiles.size() - 1));
            for (size_t i = 1; i < ts.tiles.size(); ++i)
            {
                const auto& t = ts.tiles[i];
                w.i32(t.source); w.i32(t.ix); w.i32(t.iy); w.u8(t.walkable ? 1 : 0);
            }
            w.u32((uint32_t)ts.properties.size());
            for (const auto& pd : ts.properties)
            {
                w.str(pd.name); w.str(pd.type); w.str(pd.default_value);
            }
            w.u32((uint32_t)(ts.terrains.size() - 1));
            for (size_t i = 1; i < ts.terrains.size(); ++i)
            {
                const auto& t = ts.terrains[i];
                w.str(t.name); w.u8((uint8_t)t.kind);
                w.i32(t.source); w.i32(t.ix); w.i32(t.iy); w.u8(t.walkable ? 1 : 0);
                w.u32((uint32_t)t.variant_tiles.size());
                for (int32_t v : t.variant_tiles) w.i32(v);
            }
            uint32_t tileprop_count = 0;
            for (const auto& [tid, kvs] : ts.tile_properties) tileprop_count += (uint32_t)(1 + kvs.size());
            w.u32(tileprop_count);
            for (const auto& [tid, kvs] : ts.tile_properties)
            {
                w.i32(tid); w.u32((uint32_t)kvs.size());
                for (const auto& [k, v] : kvs) { w.str(k); w.str(v); }
            }

            FILE* f = fopen(resolve_runtime_prefix(path).c_str(), "wb");
            if (f == nullptr)
                return false;
            bool written = fwrite(w.b.data(), 1, w.b.size(), f) == w.b.size();
            fclose(f);
            return written;
        }

        inline std::shared_ptr<TilesetDocument> load_tileset_from(const std::string& path)
        {
            jeecs_file* f = jeecs_file_open(path.c_str());
            if (f == nullptr)
                return nullptr;
            std::vector<char> buf(f->m_file_length);
            size_t got = jeecs_file_read(buf.data(), 1, buf.size(), f);
            jeecs_file_close(f);
            if (got != buf.size())
                return nullptr;

            bin_reader r{ buf.data(), buf.size(), 0, true };
            auto ts = std::make_shared<TilesetDocument>();
            ts->path = path;
            if (r.u32() != TILESET_MAGIC) return nullptr;
            ts->name = r.str();
            uint32_t src_n = r.u32();
            for (uint32_t i = 0; i < src_n && r.ok; ++i)
            {
                SourceTexture s;
                s.path = r.str();
                s.tile_px = r.i32(); s.xcount = r.i32(); s.ycount = r.i32();
                s.tex_w = r.i32(); s.tex_h = r.i32();
                ts->sources.push_back(std::move(s));
            }
            ts->tiles.push_back(TileDef{}); // id 0 占位
            uint32_t tile_n = r.u32();
            for (uint32_t i = 0; i < tile_n && r.ok; ++i)
            {
                TileDef t;
                t.source = r.i32(); t.ix = r.i32(); t.iy = r.i32();
                t.walkable = r.u8() != 0;
                ts->tiles.push_back(t);
            }
            uint32_t prop_n = r.u32();
            for (uint32_t i = 0; i < prop_n && r.ok; ++i)
            {
                PropertyDef pd;
                pd.name = r.str(); pd.type = r.str(); pd.default_value = r.str();
                ts->properties.push_back(std::move(pd));
            }
            ts->terrains.push_back(TerrainDef{}); // id 0 占位
            uint32_t terr_n = r.u32();
            for (uint32_t i = 0; i < terr_n && r.ok; ++i)
            {
                TerrainDef t;
                t.name = r.str(); t.kind = r.u8();
                t.source = r.i32(); t.ix = r.i32(); t.iy = r.i32();
                t.walkable = r.u8() != 0;
                uint32_t vn = r.u32();
                for (uint32_t k = 0; k < vn && r.ok; ++k) t.variant_tiles.push_back(r.i32());
                ts->terrains.push_back(std::move(t));
            }
            uint32_t tp_n = r.u32();
            for (uint32_t i = 0; i < tp_n && r.ok; ++i)
            {
                int32_t tid = r.i32();
                uint32_t kv_n = r.u32();
                for (uint32_t k = 0; k < kv_n && r.ok; ++k)
                {
                    std::string key = r.str(), val = r.str();
                    ts->tile_properties[tid][key] = val;
                }
            }
            if (!r.ok)
                return nullptr;
            ts->rebuild_flat_index();
            return ts;
        }

        inline bool save_map_to(const MapDocument& m, const std::string& path)
        {
            bin_writer w;
            w.u32(MAP_MAGIC);
            w.i32(m.width); w.i32(m.height); w.i32(m.tile_px);
            w.u32((uint32_t)m.tilesets.size());
            for (const auto& p : m.tilesets) w.str(p);
            w.u32((uint32_t)m.layers.size());
            for (const auto& l : m.layers)
            {
                w.str(l.name); w.u8(l.visible ? 1 : 0); w.u8(l.locked ? 1 : 0);
                w.u32((uint32_t)l.grid.size());
                for (int32_t v : l.grid) w.i32(v);
            }
            w.u32((uint32_t)m.cell_properties.size());
            for (const auto& [key, kvs] : m.cell_properties)
            {
                w.i32(key.first); w.i32(key.second);
                w.u32((uint32_t)kvs.size());
                for (const auto& [k, v] : kvs) { w.str(k); w.str(v); }
            }
            FILE* f = fopen(resolve_runtime_prefix(path).c_str(), "wb");
            if (f == nullptr)
                return false;
            bool written = fwrite(w.b.data(), 1, w.b.size(), f) == w.b.size();
            fclose(f);
            return written;
        }

        inline std::shared_ptr<MapDocument> load_map_from(const std::string& path)
        {
            jeecs_file* f = jeecs_file_open(path.c_str());
            if (f == nullptr)
                return nullptr;
            std::vector<char> buf(f->m_file_length);
            size_t got = jeecs_file_read(buf.data(), 1, buf.size(), f);
            jeecs_file_close(f);
            if (got != buf.size())
                return nullptr;

            bin_reader r{ buf.data(), buf.size(), 0, true };
            auto m = std::make_shared<MapDocument>();
            m->path = path;
            if (r.u32() != MAP_MAGIC) return nullptr;
            m->width = r.i32(); m->height = r.i32(); m->tile_px = r.i32();
            if (m->width < 0 || m->height < 0 || m->tile_px <= 0
                || m->width > 65536 || m->height > 65536)
                return nullptr;
            uint32_t ts_n = r.u32();
            for (uint32_t i = 0; i < ts_n && r.ok; ++i) m->tilesets.push_back(r.str());
            uint32_t layer_n = r.u32();
            for (uint32_t i = 0; i < layer_n && r.ok; ++i)
            {
                MapLayer l;
                l.name = r.str();
                l.visible = r.u8() != 0;
                l.locked = r.u8() != 0;
                uint32_t cells = r.u32();
                if (cells != (uint32_t)(m->width * m->height)) { r.ok = false; break; }
                l.grid.resize(cells);
                for (uint32_t k = 0; k < cells && r.ok; ++k) l.grid[k] = r.i32();
                m->layers.push_back(std::move(l));
            }
            uint32_t cp_n = r.u32();
            for (uint32_t i = 0; i < cp_n && r.ok; ++i)
            {
                int32_t layer = r.i32(), cell = r.i32();
                uint32_t kv_n = r.u32();
                for (uint32_t k = 0; k < kv_n && r.ok; ++k)
                {
                    std::string key = r.str(), val = r.str();
                    m->cell_properties[{ layer, cell }][key] = val;
                }
            }
            if (!r.ok)
                return nullptr;
            return m;
        }

        // ==================== 文档注册表 ====================
        // 进程内全局唯一：按路径共享文档实例，编辑器与世界经由同一份
        // 数据实现"未保存修改实时同步"。失败路径只尝试一次，避免逐帧
        // 重试造成日志刷屏；保存/重载会清除失败记录。
        class documents
        {
        public:
            static documents& inst()
            {
                static documents d;
                return d;
            }

            std::vector<std::shared_ptr<MapDocument>> maps;
            std::unordered_map<std::string, int32_t> maps_by_path;
            std::set<std::string> maps_failed;

            std::vector<std::shared_ptr<TilesetDocument>> tilesets;
            std::unordered_map<std::string, int32_t> tilesets_by_path;
            std::set<std::string> tilesets_failed;

            // 注册表键规范化：@/ 与 !/ 前缀解析为真实目录，反斜杠统一为正斜杠。
            // 无前缀的相对路径（浏览器工作目录形态）按运行时目录补全，
            // 与 jeecs_file_open 的解析一致。同一文件的不同形态字符串
            // 必须映射到同一文档实例，否则编辑器两侧会各自持有独立副本。
            std::string canonical_key(const std::string& path) const
            {
                std::string s = resolve_runtime_prefix(path);
                bool absolute = s.size() >= 2 && (s[1] == ':' || s[0] == '/');
                if (!absolute)
                    s = std::string(::jeecs_file_get_runtime_path()) + '/' + s;
                for (auto& ch : s)
                    if (ch == '\\')
                        ch = '/';
                return s;
            }

            MapDocument* map(int32_t id)
            {
                if (id <= 0 || id > (int32_t)maps.size() || maps[id - 1] == nullptr)
                    return nullptr;
                return maps[id - 1].get();
            }
            TilesetDocument* tileset(int32_t id)
            {
                if (id <= 0 || id > (int32_t)tilesets.size() || tilesets[id - 1] == nullptr)
                    return nullptr;
                return tilesets[id - 1].get();
            }

            int32_t register_map(std::shared_ptr<MapDocument> doc)
            {
                maps.push_back(std::move(doc));
                return (int32_t)maps.size();
            }
            int32_t register_tileset(std::shared_ptr<TilesetDocument> doc)
            {
                tilesets.push_back(std::move(doc));
                return (int32_t)tilesets.size();
            }

            void bind_map_path(const std::string& path, int32_t id)
            {
                maps_by_path[path] = id;
                maps_failed.erase(path);
            }
            void bind_tileset_path(const std::string& path, int32_t id)
            {
                tilesets_by_path[path] = id;
                tilesets_failed.erase(path);
            }

            // 打开（或复用缓存）地图文档；失败记录进 maps_failed
            std::pair<int32_t, MapDocument*> open_map(const std::string& path)
            {
                if (path.empty())
                    return { 0, nullptr };
                std::string key = canonical_key(path);
                auto fnd = maps_by_path.find(key);
                if (fnd != maps_by_path.end())
                {
                    MapDocument* d = map(fnd->second);
                    if (d != nullptr)
                        return { fnd->second, d };
                }
                if (maps_failed.count(key) != 0)
                    return { 0, nullptr };
                auto doc = load_map_from(path);
                if (doc == nullptr)
                {
                    maps_failed.insert(key);
                    debug::logerr("Tilemap: unable to open map file '%s'.", path.c_str());
                    return { 0, nullptr };
                }
                int32_t id = register_map(doc);
                bind_map_path(key, id);
                return { id, doc.get() };
            }

            std::pair<int32_t, TilesetDocument*> open_tileset(const std::string& path)
            {
                if (path.empty())
                    return { 0, nullptr };
                std::string key = canonical_key(path);
                auto fnd = tilesets_by_path.find(key);
                if (fnd != tilesets_by_path.end())
                {
                    TilesetDocument* d = tileset(fnd->second);
                    if (d != nullptr)
                        return { fnd->second, d };
                }
                if (tilesets_failed.count(key) != 0)
                    return { 0, nullptr };
                auto doc = load_tileset_from(path);
                if (doc == nullptr)
                {
                    tilesets_failed.insert(key);
                    debug::logerr("Tilemap: unable to open tileset file '%s'.", path.c_str());
                    return { 0, nullptr };
                }
                int32_t id = register_tileset(doc);
                bind_tileset_path(key, id);
                return { id, doc.get() };
            }

            void reload_map(const std::string& path)
            {
                maps_by_path.erase(canonical_key(path));
                maps_failed.erase(canonical_key(path));
            }
            void reload_tileset(const std::string& path)
            {
                tilesets_by_path.erase(canonical_key(path));
                tilesets_failed.erase(canonical_key(path));
            }

            // 保证地图文档的图集解析与注册表中的最新文档一致
            // （处理图集被重载/替换的情况），并返回引用计数不变的共享指针。
            void ensure_tilesets(MapDocument& m)
            {
                if (m.resolved.size() != m.tilesets.size())
                    m.resolved.assign(m.tilesets.size(), nullptr);
                for (size_t i = 0; i < m.tilesets.size(); ++i)
                {
                    auto [id, live] = open_tileset(m.tilesets[i]);
                    if (live == nullptr)
                    {
                        if (m.resolved[i] != nullptr)
                            m.resolved[i].reset();
                        continue;
                    }
                    // 与注册表当前实例保持同一份（open 命中缓存即同指针）
                    m.resolved[i] = tilesets[id - 1];
                }
            }

            // 地图 + 其图集的合并版本戳：任一变化都会改变
            uint64_t map_stamp(const MapDocument& m) const
            {
                uint64_t stamp = m.version * 0x9E3779B97F4A7C15ull;
                for (const auto& ts : m.resolved)
                    stamp = stamp * 31 + (ts ? ts->version : 0) + 0x2545F491;
                return stamp;
            }
        };

        // ==================== 自动图块 ====================
        // 邻接掩码位（与旧版编辑器 WrappingTileWay 一致）
        constexpr int NB_U = 1, NB_D = 2, NB_L = 4, NB_R = 8;
        constexpr int NB_UL = 16, NB_LD = 32, NB_RU = 64, NB_RD = 128;

        // 计算某格地形的 8 邻接掩码：邻居与该格存储值完全相同才算连接
        inline int compute_mask(const MapDocument& m, int32_t layer, int32_t x, int32_t y, int32_t encoded)
        {
            const auto& grid = m.layers[layer].grid;
            auto same = [&](int32_t nx, int32_t ny) -> bool
            {
                if (nx < 0 || ny < 0 || nx >= m.width || ny >= m.height)
                    return false;
                return grid[ny * m.width + nx] == encoded;
            };
            int mask = 0;
            if (same(x, y - 1)) mask |= NB_U;
            if (same(x, y + 1)) mask |= NB_D;
            if (same(x - 1, y)) mask |= NB_L;
            if (same(x + 1, y)) mask |= NB_R;
            if (same(x - 1, y - 1)) mask |= NB_UL;
            if (same(x - 1, y + 1)) mask |= NB_LD;
            if (same(x + 1, y - 1)) mask |= NB_RU;
            if (same(x + 1, y + 1)) mask |= NB_RD;
            return mask;
        }

        // RPGMaker 式四象限：地形占据锚点右侧 2 格、下方 3 格？——否，
        // 条带位于锚点 (ix,iy) 起的 2(宽)×3(高) 图集格，四象限自条带的
        // 4×6 半格网格中取样。下表为各角 5 种邻接状态到半格 (列,行) 的
        // 映射，自旧版编辑器（f8a48343^ 的 tilemap/main.wo）像素采样逻辑
        // 解码移植。状态序：[两侧+斜角, 两侧无斜角, 第一方向边, 第二方向边, 无邻接]
        // LU/RU 的“第一方向”为 U（上下中的上）；LD/RD 为 D；第二方向 L/R。
        struct half_cell { int col, row; };
        inline half_cell rpgmaker_quadrant(int corner /*0=LU 1=RU 2=LD 3=RD*/, int state)
        {
            static constexpr int TABLE[4][5][2] = {
                /*LU*/ { {2,4},{2,0},{0,4},{2,2},{0,2} },
                /*RU*/ { {1,4},{3,0},{3,4},{1,2},{3,2} },
                /*LD*/ { {2,3},{2,1},{0,3},{2,5},{0,5} },
                /*RD*/ { {1,3},{3,1},{3,3},{1,5},{3,5} },
            };
            return { TABLE[corner][state][0], TABLE[corner][state][1] };
        }
        // RPGMaker VX/VX Ace 式：2(宽)x3(高) 块的 4x6 半格取件表。
        // 表值 = (列,行)。自 mkxp-z autotileVXRectsA(48 变体权威表)机械推导：
        // 除孤块(mask==0，mkxp 变体47=锚点整块)外，各件槽位由"件频次==签名计数"
        // 唯一确定(13内/13凹/8边/8边/5外, 1=备用锚点件)，边件手性经 16 组合
        // 全掩码验证为唯一解(255/255 掩码命中 mkxp 行, 47/47 行全覆盖)。
        // 状态序与 corner_state 一致：[内部, 凹角, 第一方向边, 第二方向边, 外角]
        //   LU/RU 第一方向为 U；LD/RD 为 D；第二方向 L/R。
        inline half_cell rpgmaker_vx_quadrant(int corner /*0=LU 1=RU 2=LD 3=RD*/, int state)
        {
            static constexpr int TABLE[4][5][2] = {
                /*LU*/ { {2,4},{2,0},{0,4},{2,2},{0,2} },
                /*RU*/ { {1,4},{3,0},{3,4},{1,2},{3,2} },
                /*LD*/ { {2,3},{2,1},{0,3},{2,5},{0,5} },
                /*RD*/ { {1,3},{3,1},{3,3},{1,5},{3,5} },
            };
            return { TABLE[corner][state][0], TABLE[corner][state][1] };
        }
        inline int corner_state(int mask, int o1, int o2, int diag)
        {
            if ((mask & o1) && (mask & o2))
                return (mask & diag) ? 0 : 1;
            if (mask & o1) return 2;
            if (mask & o2) return 3;
            return 4;
        }

        // blob47 归约：256 个 8bit 掩码 -> 47 个变体号。
        // 角状态无关的对角位被吸收；枚举序 = 正交掩码 0..15 为主序、
        // 相关对角组合为次序的首次出现序（即本引擎的规范 47 变体序）。
        inline const std::vector<uint8_t>& blob47_table()
        {
            static std::vector<uint8_t> table = []
            {
                std::vector<uint8_t> t(256, 0);
                std::map<std::pair<int, int>, uint8_t> variant_ids;
                uint8_t next = 0;
                for (int omask = 0; omask < 16; ++omask)
                {
                    bool u = omask & NB_U, d = omask & NB_D, l = omask & NB_L, r = omask & NB_R;
                    // 仅当两个相邻正交位都存在时对应对角位才参与区分
                    int diag_relevant = 0;
                    if (u && l) diag_relevant |= NB_UL;
                    if (d && l) diag_relevant |= NB_LD;
                    if (u && r) diag_relevant |= NB_RU;
                    if (d && r) diag_relevant |= NB_RD;
                    for (int dmask = 0; dmask < 16; ++dmask)
                    {
                        // 把 4 个对角位映射到 dmask 的位 0..3 (UL,LD,RU,RD)
                        int real = 0;
                        if (dmask & 1) real |= NB_UL;
                        if (dmask & 2) real |= NB_LD;
                        if (dmask & 4) real |= NB_RU;
                        if (dmask & 8) real |= NB_RD;
                        int key_d = real & diag_relevant;
                        auto key = std::make_pair(omask, key_d);
                        auto fnd = variant_ids.find(key);
                        if (fnd == variant_ids.end())
                        {
                            // 检查是否与更早的正交组合等价（角状态相同）：
                            // 直接以四角状态为键去重，保证恰好 47 个
                            int su = 0, sd = 0, sl = 0, sr = 0;
                            // 每角的 2bit 状态：0凸 1边 2凹 3内
                            auto corner = [](bool o1, bool o2, bool dg) -> int
                            {
                                if (o1 && o2) return dg ? 3 : 2;
                                if (o1 || o2) return 1;
                                return 0;
                            };
                            std::tuple<int, int, int, int> skey(
                                corner(u, l, (key_d & NB_UL) != 0),
                                corner(u, r, (key_d & NB_RU) != 0),
                                corner(d, l, (key_d & NB_LD) != 0),
                                corner(d, r, (key_d & NB_RD) != 0));
                            static std::map<std::tuple<int, int, int, int>, uint8_t> states;
                            auto sfnd = states.find(skey);
                            if (sfnd == states.end())
                            {
                                states[skey] = next;
                                t[omask | (real & 0xF0)] = next;
                                variant_ids[key] = next;
                                ++next;
                            }
                            else
                            {
                                t[omask | (real & 0xF0)] = sfnd->second;
                                variant_ids[key] = sfnd->second;
                            }
                        }
                        else
                            t[omask | (real & 0xF0)] = fnd->second;
                    }
                }
                return t;
            }();
            return table;
        }

        // ==================== 属性与通行性 ====================
        inline const std::string* resolve_property(
            MapDocument* doc, int32_t layer, int32_t x, int32_t y, const std::string& name)
        {
            if (doc == nullptr || !doc->in_bounds(layer, x, y))
                return nullptr;
            // API 直调时无系统代管图集解析，此处兜底（幂等）
            documents::inst().ensure_tilesets(*doc);
            // 1. 逐格覆盖
            auto cit = doc->cell_properties.find({ layer, x + y * doc->width });
            if (cit != doc->cell_properties.end())
            {
                auto fnd = cit->second.find(name);
                if (fnd != cit->second.end())
                    return &fnd->second;
            }
            int32_t v = doc->layers[layer].grid[y * doc->width + x];
            if (v != 0)
            {
                int32_t ts_idx = 0, local = 0;
                decode_tile_value(v, &ts_idx, &local);
                if (ts_idx >= 0 && ts_idx < (int32_t)doc->resolved.size()
                    && doc->resolved[ts_idx] != nullptr)
                {
                    const auto* ts = doc->resolved[ts_idx].get();
                    const std::map<std::string, std::string>* defaults = nullptr;
                    if (v < 0)
                    {
                        if (local < (int32_t)ts->terrains.size())
                            defaults = &ts->terrains[local].properties;
                    }
                    else if (local < (int32_t)ts->tiles.size())
                    {
                        auto tit = ts->tile_properties.find(local);
                        if (tit != ts->tile_properties.end())
                            defaults = &tit->second;
                    }
                    if (defaults != nullptr)
                    {
                        auto fnd = defaults->find(name);
                        if (fnd != defaults->end())
                            return &fnd->second;
                    }
                    // 3. schema 默认值
                    for (const auto& pd : ts->properties)
                        if (pd.name == name)
                            return &pd.default_value;
                }
            }
            return nullptr;
        }

        inline bool walkable_on(MapDocument* doc, int32_t layer, int32_t x, int32_t y)
        {
            if (doc == nullptr || !doc->in_bounds(layer, x, y))
                return false;
            documents::inst().ensure_tilesets(*doc);
            int32_t v = doc->layers[layer].grid[y * doc->width + x];
            if (v == 0)
                return true;
            int32_t ts_idx = 0, local = 0;
            decode_tile_value(v, &ts_idx, &local);
            if (ts_idx < 0 || ts_idx >= (int32_t)doc->resolved.size()
                || doc->resolved[ts_idx] == nullptr)
                return true;
            const auto* ts = doc->resolved[ts_idx].get();
            if (v < 0)
                return local < (int32_t)ts->terrains.size() ? ts->terrains[local].walkable : true;
            return local < (int32_t)ts->tiles.size() ? ts->tiles[local].walkable : true;
        }

        // 从所有缓存地图中清除引用了指定瓦片/地形的格子（删除定义时调用）
        inline void purge_value_from_maps(int32_t tileset_idx, int32_t local_id, bool is_terrain)
        {
            int32_t value = is_terrain
                ? encode_terrain_value(tileset_idx, local_id)
                : encode_tile_value(tileset_idx, local_id);
            for (auto& doc : documents::inst().maps)
            {
                if (doc == nullptr)
                    continue;
                bool changed = false;
                for (auto& layer : doc->layers)
                {
                    for (auto& v : layer.grid)
                    {
                        if (v == value)
                        {
                            v = 0;
                            changed = true;
                        }
                    }
                }
                if (changed)
                    ++doc->version;
            }
        }
    }

    // ==================================================================
    // TilemapSystem：把地图文档实例化为分块渲染子实体
    // ==================================================================
    struct TilemapSystem : public game_system
    {
        static constexpr size_t CHUNK = 32;         // 分块边长（格）
        static constexpr size_t FLOATS_PER_VERTEX = 8;   // pos3 + uv2 + normal3
        static constexpr size_t VERTS_PER_CELL = 16;     // 每格 4 个子四边形槽位
        static constexpr size_t FLOATS_PER_CELL = VERTS_PER_CELL * FLOATS_PER_VERTEX;

        struct part_entry
        {
            je_GameEntity entity{};
            int32_t layer = 0, chunk_x = 0, chunk_y = 0, source = 0;
            uint64_t built_stamp = 0;
        };
        // root entity id -> 分片表（key: 分片实体 id）
        std::unordered_map<uint32_t, std::map<uint32_t, part_entry>> _m_roots;
        std::vector<float> _m_staging;

        TilemapSystem(game_world w)
            : game_system(w)
        {
            _m_staging.assign(CHUNK * CHUNK * FLOATS_PER_CELL, 0.f);
        }

        static int64_t pack_key(int32_t layer, int32_t cx, int32_t cy, int32_t source)
        {
            return ((int64_t)layer << 46) | ((int64_t)(uint32_t)cx << 34)
                | ((int64_t)(uint32_t)cy << 22) | (uint32_t)source;
        }

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

        // UV 矩形（图集源纹理网格坐标 -> 引擎 UV；v0 下 v1 上）
        static void tile_uv(const Tilemap::SourceTexture& src, int32_t ix, int32_t iy,
            float& u0, float& v0_bot, float& u1, float& v1_top)
        {
            float px = (float)(ix * src.tile_px), py = (float)(iy * src.tile_px);
            float sz = (float)src.tile_px;
            float w = (float)(src.tex_w ? src.tex_w : src.xcount * src.tile_px);
            float h = (float)(src.tex_h ? src.tex_h : src.ycount * src.tile_px);
            u0 = px / w;
            u1 = (px + sz) / w;
            v1_top = 1.f - py / h;
            v0_bot = 1.f - (py + sz) / h;
        }

        // 把一格写入 staging（已保证该格属于此 source），返回是否写入
        bool emit_cell(float* dst, Tilemap::MapDocument& doc, int32_t layer,
            int32_t x, int32_t y)
        {
            int32_t v = doc.layers[layer].grid[y * doc.width + x];
            if (v == 0)
                return false;

            int32_t ts_idx = 0, local = 0;
            Tilemap::decode_tile_value(v, &ts_idx, &local);
            if (ts_idx < 0 || ts_idx >= (int32_t)doc.resolved.size()
                || doc.resolved[ts_idx] == nullptr)
                return false;
            const auto* ts = doc.resolved[ts_idx].get();

            const float tpx = (float)doc.tile_px;
            const float x0 = x * tpx, x1 = x0 + tpx;
            const float ytop = -y * tpx, ybot = ytop - tpx;

            // 先全部写成退化四边形（零面积，不产生像素）
            for (size_t q = 0; q < 4; ++q)
                write_quad(dst + q * 4 * FLOATS_PER_VERTEX, x0, ytop, x0, ytop, 0.f, 0.f, 0.f, 0.f);

            auto emit_full_tile = [&](const Tilemap::SourceTexture& src, int32_t ix, int32_t iy)
            {
                float u0, vb, u1, vt;
                tile_uv(src, ix, iy, u0, vb, u1, vt);
                write_quad(dst, x0, ybot, x1, ytop, u0, vb, u1, vt);
            };

            if (v > 0)
            {
                if (local <= 0 || local >= (int32_t)ts->tiles.size())
                    return false;
                const auto& t = ts->tiles[local];
                if (t.source < 0 || t.source >= (int32_t)ts->sources.size())
                    return false;
                emit_full_tile(ts->sources[t.source], t.ix, t.iy);
                return true;
            }

            // 自动图块地形
            if (local <= 0 || local >= (int32_t)ts->terrains.size())
                return false;
            const auto& terrain = ts->terrains[local];
            if (terrain.source < 0 || terrain.source >= (int32_t)ts->sources.size())
                return false;
            const auto& src = ts->sources[terrain.source];

            if (terrain.kind == 1 || terrain.kind == 3)
            {
                // RPGMaker 四象限（XP 式条带 / VX 式 2x3 块）：
                // 按邻接掩码为四个角各取一个半格
                int mask = Tilemap::compute_mask(doc, layer, x, y, v);
                if (terrain.kind == 3 && mask == 0)
                {
                    // VX 孤块：mkxp 变体47 = 锚点整块
                    emit_full_tile(src, terrain.ix, terrain.iy);
                    return true;
                }
                const float half = tpx * 0.5f;
                const float xm = x0 + half, ym = ybot + half;
                const float shalf = src.tile_px * 0.5f;
                float w = (float)(src.tex_w ? src.tex_w : src.xcount * src.tile_px);
                float h = (float)(src.tex_h ? src.tex_h : src.ycount * src.tile_px);
                // 半格 (col,row) 的 UV：row 自条带锚点顶部向下增长
                auto half_uv = [&](int col, int row, float& u0, float& vb, float& u1, float& vt)
                {
                    float px = terrain.ix * src.tile_px + col * shalf;
                    float py = terrain.iy * src.tile_px + row * shalf;
                    u0 = px / w; u1 = (px + shalf) / w;
                    vt = 1.f - py / h;
                    vb = 1.f - (py + shalf) / h;
                };
                // 按解析器种类选择半格表（XP 式条带 / VX 式 2x3 块）
                #define JE_TL_QUAD(CORNER, O1, O2, DIAG)                     (terrain.kind == 1                         ? Tilemap::rpgmaker_quadrant(CORNER,                             Tilemap::corner_state(mask, O1, O2, DIAG))                         : Tilemap::rpgmaker_vx_quadrant(CORNER,                             Tilemap::corner_state(mask, O1, O2, DIAG)))
                float u0, vb, u1, vt;
                // LU（角 0：邻接 U/L/UL）
                half_uv(JE_TL_QUAD(0, Tilemap::NB_U, Tilemap::NB_L, Tilemap::NB_UL).col,
                    JE_TL_QUAD(0, Tilemap::NB_U, Tilemap::NB_L, Tilemap::NB_UL).row,
                    u0, vb, u1, vt);
                write_quad(dst + 0 * 4 * FLOATS_PER_VERTEX, x0, ym, xm, ytop, u0, vb, u1, vt);
                // RU（角 1：U/R/RU）
                half_uv(JE_TL_QUAD(1, Tilemap::NB_U, Tilemap::NB_R, Tilemap::NB_RU).col,
                    JE_TL_QUAD(1, Tilemap::NB_U, Tilemap::NB_R, Tilemap::NB_RU).row,
                    u0, vb, u1, vt);
                write_quad(dst + 1 * 4 * FLOATS_PER_VERTEX, xm, ym, x1, ytop, u0, vb, u1, vt);
                // LD（角 2：D/L/LD）
                half_uv(JE_TL_QUAD(2, Tilemap::NB_D, Tilemap::NB_L, Tilemap::NB_LD).col,
                    JE_TL_QUAD(2, Tilemap::NB_D, Tilemap::NB_L, Tilemap::NB_LD).row,
                    u0, vb, u1, vt);
                write_quad(dst + 2 * 4 * FLOATS_PER_VERTEX, x0, ybot, xm, ym, u0, vb, u1, vt);
                // RD（角 3：D/R/RD）
                half_uv(JE_TL_QUAD(3, Tilemap::NB_D, Tilemap::NB_R, Tilemap::NB_RD).col,
                    JE_TL_QUAD(3, Tilemap::NB_D, Tilemap::NB_R, Tilemap::NB_RD).row,
                    u0, vb, u1, vt);
                write_quad(dst + 3 * 4 * FLOATS_PER_VERTEX, xm, ybot, x1, ym, u0, vb, u1, vt);
                #undef JE_TL_QUAD
                return true;
            }

            // 单块：直接取锚点整块（不依赖瓦片定义表）
            if (terrain.kind == 0)
            {
                emit_full_tile(src, terrain.ix, terrain.iy);
                return true;
            }

            // blob47：按邻接掩码解析为一个整块变体
            int32_t variant_tile = 0;
            {
                int mask = Tilemap::compute_mask(doc, layer, x, y, v);
                uint8_t variant = Tilemap::blob47_table()[(uint8_t)(mask & 0xFF)];
                if ((size_t)variant < terrain.variant_tiles.size())
                    variant_tile = terrain.variant_tiles[variant];
                else
                {
                    // 默认布局：自锚点起行优先连续取 47 块
                    int32_t flat = terrain.iy * src.xcount + terrain.ix + variant;
                    variant_tile = ts->tile_at_flat(terrain.source, flat);
                }
            }
            if (variant_tile > 0 && variant_tile < (int32_t)ts->tiles.size())
            {
                const auto& t = ts->tiles[variant_tile];
                if (t.source >= 0 && t.source < (int32_t)ts->sources.size())
                {
                    emit_full_tile(ts->sources[t.source], t.ix, t.iy);
                    return true;
                }
            }
            return true;
        }

        game_entity create_part_entity(game_entity root, int32_t layer, int32_t z_layer,
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
            // 图层沿 +z 逐层抬升（近摄像机者后绘制）
            pos->pos = math::vec3(0.f, 0.f, 0.002f * (float)z_layer);
            part.get_component<Renderer::Rendqueue>()->rend_queue = layer;

            auto* shaders = part.get_component<Renderer::Shaders>();
            if (auto shad = shared_tile_shader())
                shaders->shaders.push_back(shad.value());

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

        void rebuild_part(Tilemap::MapDocument& doc, part_entry& part)
        {
            auto world = get_world();
            game_entity e{ part.entity };
            auto* shape = e.get_component<Renderer::Shape>();
            if (shape == nullptr || !shape->vertex.has_value())
                return;

            memset(_m_staging.data(), 0, _m_staging.size() * sizeof(float));
            size_t used_cells = 0;
            const int32_t bx = part.chunk_x * (int32_t)CHUNK;
            const int32_t by = part.chunk_y * (int32_t)CHUNK;

            for (int32_t ly = 0; ly < (int32_t)CHUNK; ++ly)
            {
                int32_t y = by + ly;
                if (y >= doc.height) break;
                for (int32_t lx = 0; lx < (int32_t)CHUNK; ++lx)
                {
                    int32_t x = bx + lx;
                    if (x >= doc.width) break;
                    int32_t v = doc.layers[part.layer].grid[y * doc.width + x];
                    if (v == 0)
                        continue;
                    // 该格须属于此分片的 (图集, 源纹理)
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
                    // source 打包：图集下标 * 1024 + 源纹理下标
                    if (ts_idx * 1024 + src_idx != part.source)
                        continue;

                    if (emit_cell(_m_staging.data() + used_cells * FLOATS_PER_CELL,
                        doc, part.layer, x, y))
                        ++used_cells;
                }
            }

            jegl_vertex* raw = shape->vertex->get()->resource();
            shape->vertex->get()->update_buffer(
                _m_staging.data(),
                _m_staging.size() * sizeof(float),
                used_cells * 4 * 6);

            // 分块包围盒（局部空间：地图向 +x 与 -y 展开）
            const float tpx = (float)doc.tile_px;
            raw->m_x_min = bx * tpx;
            raw->m_x_max = (std::min<int32_t>(bx + (int32_t)CHUNK, doc.width)) * tpx;
            raw->m_y_max = -(by * tpx);
            raw->m_y_min = -(std::min<int32_t>(by + (int32_t)CHUNK, doc.height)) * tpx;
            raw->m_z_min = -0.01f;
            raw->m_z_max = 0.01f + 0.002f * (float)part.layer;
        }

        void Update()
        {
            auto world = get_world();
            auto& docs = Tilemap::documents::inst();

            // 1. 收集地图根实体与文档
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
                auto [id, doc] = docs.open_map(path);
                r.doc = doc;
            }

            // 2. 回收孤儿分片（所有者不存在或不再持有 Map 组件）
            std::vector<je_GameEntity> dead_parts;
            for (auto&& [e, part] :
                query_entity<view typesof(Tilemap::RenderPart&)>())
            {
                if (alive_root_ids.count(part.owner_id) == 0)
                    dead_parts.push_back(e._m_raw);
            }
            for (auto& raw : dead_parts)
            {
                world.remove_entity(game_entity{ raw });
                for (auto& [root_id, parts] : _m_roots)
                    parts.erase(raw._m_id);
            }

            // 3. 逐根同步分片集合与版本
            for (auto& r : roots)
            {
                uint32_t root_id = r.raw._m_id;
                auto& parts = _m_roots[root_id];
                game_entity root{ r.raw };

                // 计算需要的分片：(layer, chunk, source) -> 存在
                std::set<int64_t> needed;
                uint64_t stamp = 0;
                if (r.doc != nullptr)
                {
                    docs.ensure_tilesets(*r.doc);
                    stamp = docs.map_stamp(*r.doc);
                    auto& doc = *r.doc;
                    for (size_t li = 0; li < doc.layers.size(); ++li)
                    {
                        if (!doc.layers[li].visible)
                            continue;
                        // (chunk_y<<16|chunk_x) -> 使用的 source 集合
                        std::map<int32_t, std::set<int32_t>> usage;
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
                                usage[((y / (int32_t)CHUNK) << 16) | (uint32_t)(x / (int32_t)CHUNK)]
                                    .insert(ts_idx * 1024 + src_idx);
                            }
                        }
                        for (const auto& [chunk_key, sources] : usage)
                        {
                            int32_t cy = chunk_key >> 16, cx = chunk_key & 0xFFFF;
                            for (int32_t src : sources)
                                needed.insert(pack_key((int32_t)li, cx, cy, src));
                        }
                    }
                }

                // 删除不再需要的分片
                std::vector<uint32_t> removed;
                for (auto& [pid, entry] : parts)
                {
                    if (needed.count(pack_key(entry.layer, entry.chunk_x, entry.chunk_y, entry.source)) == 0)
                        removed.push_back(pid);
                }
                for (uint32_t pid : removed)
                {
                    auto& entry = parts[pid];
                    world.remove_entity(game_entity{ entry.entity });
                    parts.erase(pid);
                }

                // 重建/刷新保留与新增的分片
                std::map<int64_t, part_entry*> by_key;
                for (auto& [pid, entry] : parts)
                    by_key[pack_key(entry.layer, entry.chunk_x, entry.chunk_y, entry.source)] = &entry;

                for (int64_t key : needed)
                {
                    auto fnd = by_key.find(key);
                    if (fnd != by_key.end())
                    {
                        if (r.doc != nullptr && fnd->second->built_stamp != stamp)
                        {
                            rebuild_part(*r.doc, *fnd->second);
                            fnd->second->built_stamp = stamp;
                        }
                        continue;
                    }
                    // 解包 key
                    int32_t src = (int32_t)((uint64_t)key & 0x3FFFFF);
                    int32_t cy = (int32_t)(((uint64_t)key >> 22) & 0xFFF);
                    int32_t cx = (int32_t)(((uint64_t)key >> 34) & 0xFFF);
                    int32_t layer = (int32_t)(((uint64_t)key >> 46) & 0xFFFF);
                    (void)cx; (void)cy;

                    int32_t ts_idx = src / 1024, src_idx = src % 1024;
                    part_entry entry{};
                    entry.layer = layer;
                    entry.chunk_x = (int32_t)(((uint64_t)key >> 34) & 0xFFF);
                    entry.chunk_y = (int32_t)(((uint64_t)key >> 22) & 0xFFF);
                    entry.source = src;
                    entry.built_stamp = 0;

                    const Tilemap::SourceTexture* src_info = nullptr;
                    if (r.doc != nullptr
                        && ts_idx < (int32_t)r.doc->resolved.size()
                        && r.doc->resolved[ts_idx] != nullptr
                        && src_idx < (int32_t)r.doc->resolved[ts_idx]->sources.size())
                        src_info = &r.doc->resolved[ts_idx]->sources[src_idx];
                    if (src_info == nullptr)
                        continue;

                    game_entity part_entity =
                        create_part_entity(root, layer, layer, *src_info);
                    auto* tag = part_entity.get_component<Tilemap::RenderPart>();
                    tag->owner_id = root_id;
                    tag->layer = layer;
                    tag->chunk_x = entry.chunk_x;
                    tag->chunk_y = entry.chunk_y;
                    tag->source = src;
                    entry.entity = part_entity._m_raw;

                    if (r.doc != nullptr)
                    {
                        rebuild_part(*r.doc, entry);
                        entry.built_stamp = stamp;
                    }
                    parts[part_entity._m_raw._m_id] = entry;
                }
            }
        }
    };
}

// ======================================================================
// je_tilemap_* JE_API 实现（声明见 include/jeecs.hpp）
// ======================================================================
jeecs::Tilemap::MapDocument* je_tilemap_map_doc(int32_t id)
{
    return jeecs::Tilemap::documents::inst().map(id);
}
jeecs::Tilemap::TilesetDocument* je_tilemap_ts_doc(int32_t id)
{
    return jeecs::Tilemap::documents::inst().tileset(id);
}

JE_API int32_t je_tilemap_open_map(const char* path)
{
    auto [id, doc] = jeecs::Tilemap::documents::inst().open_map(path ? path : "");
    return id;
}
JE_API int32_t je_tilemap_create_map(int32_t w, int32_t h, int32_t tile_px)
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
JE_API bool je_tilemap_save_map(int32_t map, const char* path)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || path == nullptr || path[0] == '\0')
        return false;
    if (!jeecs::Tilemap::save_map_to(*doc, path))
        return false;
    auto& inst = jeecs::Tilemap::documents::inst();
    if (!doc->path.empty() && doc->path != path)
        inst.maps_by_path.erase(inst.canonical_key(doc->path));
    doc->path = path;
    inst.bind_map_path(inst.canonical_key(path), map);
    return true;
}
JE_API void je_tilemap_reload_map(const char* path)
{
    jeecs::Tilemap::documents::inst().reload_map(path ? path : "");
}
JE_API void je_tilemap_map_size(int32_t map, int32_t* w, int32_t* h, int32_t* tile_px, int32_t* layer_count)
{
    auto* doc = je_tilemap_map_doc(map);
    if (w != nullptr) *w = doc ? doc->width : 0;
    if (h != nullptr) *h = doc ? doc->height : 0;
    if (tile_px != nullptr) *tile_px = doc ? doc->tile_px : 0;
    if (layer_count != nullptr) *layer_count = doc ? (int32_t)doc->layers.size() : 0;
}
JE_API const char* je_tilemap_map_path(int32_t map)
{
    auto* doc = je_tilemap_map_doc(map);
    return doc ? doc->path.c_str() : "";
}
JE_API uint64_t je_tilemap_map_version(int32_t map)
{
    auto* doc = je_tilemap_map_doc(map);
    return doc ? doc->version : 0;
}
JE_API void je_tilemap_resize_map(int32_t map, int32_t new_w, int32_t new_h)
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

JE_API int32_t je_tilemap_add_layer(int32_t map, const char* name)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr)
        return -1;
    jeecs::Tilemap::MapLayer layer;
    layer.name = name ? name : ("图层 " + std::to_string(doc->layers.size() + 1));
    layer.grid.assign((size_t)doc->width * doc->height, 0);
    doc->layers.push_back(std::move(layer));
    ++doc->version;
    return (int32_t)doc->layers.size() - 1;
}
JE_API bool je_tilemap_remove_layer(int32_t map, int32_t layer)
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
JE_API bool je_tilemap_move_layer(int32_t map, int32_t layer, int32_t new_pos)
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
JE_API void je_tilemap_get_layer(int32_t map, int32_t layer,
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
JE_API void je_tilemap_set_layer(int32_t map, int32_t layer,
    const char* name, int32_t visible, int32_t locked)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || layer < 0 || layer >= (int32_t)doc->layers.size())
        return;
    auto& l = doc->layers[layer];
    if (name != nullptr) l.name = name;
    l.visible = visible != 0;
    l.locked = locked != 0;
    ++doc->version;
}

JE_API int32_t je_tilemap_get_tile(int32_t map, int32_t layer, int32_t x, int32_t y)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || !doc->in_bounds(layer, x, y))
        return 0;
    return doc->layers[layer].grid[y * doc->width + x];
}
JE_API void je_tilemap_set_tile(int32_t map, int32_t layer, int32_t x, int32_t y, int32_t value)
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
JE_API void je_tilemap_fill_tiles(int32_t map, int32_t layer,
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

JE_API int32_t je_tilemap_tileset_count(int32_t map)
{
    auto* doc = je_tilemap_map_doc(map);
    return doc ? (int32_t)doc->tilesets.size() : 0;
}
JE_API int32_t je_tilemap_add_tileset(int32_t map, const char* path)
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
JE_API bool je_tilemap_remove_tileset(int32_t map, int32_t idx)
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
JE_API const char* je_tilemap_tileset_path(int32_t map, int32_t idx)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || idx < 0 || idx >= (int32_t)doc->tilesets.size())
        return "";
    return doc->tilesets[idx].c_str();
}

JE_API void je_tilemap_set_cell_property(int32_t map, int32_t layer,
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
JE_API const char* je_tilemap_get_cell_property(int32_t map, int32_t layer,
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
JE_API const char* je_tilemap_get_property(int32_t map, int32_t layer,
    int32_t x, int32_t y, const char* name)
{
    auto* doc = je_tilemap_map_doc(map);
    if (name == nullptr)
        return nullptr;
    const std::string* r = jeecs::Tilemap::resolve_property(doc, layer, x, y, name);
    return r == nullptr ? nullptr : r->c_str();
}
JE_API int32_t je_tilemap_is_walkable(int32_t map, int32_t x, int32_t y)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr)
        return 0;
    for (int32_t l = 0; l < (int32_t)doc->layers.size(); ++l)
        if (!jeecs::Tilemap::walkable_on(doc, l, x, y))
            return 0;
    // 出界由 walkable_on 返回 false（视为不可通行）
    for (int32_t l = 0; l < (int32_t)doc->layers.size(); ++l)
        if (doc->in_bounds(l, x, y))
            return 1;
    return 0;
}
JE_API int32_t je_tilemap_is_walkable_on(int32_t map, int32_t layer, int32_t x, int32_t y)
{
    auto* doc = je_tilemap_map_doc(map);
    return jeecs::Tilemap::walkable_on(doc, layer, x, y) ? 1 : 0;
}
JE_API int32_t je_tilemap_find_cells(int32_t map, int32_t layer, const char* name, const char* value)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || name == nullptr || value == nullptr)
        return 0;
    doc->find_result.clear();
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
                    doc->find_result.push_back({ x, y });
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
    return (int32_t)doc->find_result.size();
}
JE_API bool je_tilemap_find_get(int32_t map, int32_t index, int32_t* x, int32_t* y)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || index < 0 || index >= (int32_t)doc->find_result.size())
        return false;
    if (x != nullptr) *x = doc->find_result[index].first;
    if (y != nullptr) *y = doc->find_result[index].second;
    return true;
}

// ---------------- 图集文档 ----------------
JE_API int32_t je_tilemap_open_tileset(const char* path)
{
    auto [id, doc] = jeecs::Tilemap::documents::inst().open_tileset(path ? path : "");
    return id;
}
JE_API int32_t je_tilemap_create_tileset(const char* name)
{
    auto doc = std::make_shared<jeecs::Tilemap::TilesetDocument>();
    doc->name = name ? name : "新建图集";
    doc->tiles.push_back({});
    doc->terrains.push_back({});
    return jeecs::Tilemap::documents::inst().register_tileset(doc);
}
JE_API bool je_tilemap_save_tileset(int32_t tileset, const char* path)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || path == nullptr || path[0] == '\0')
        return false;
    if (!jeecs::Tilemap::save_tileset_to(*doc, path))
        return false;
    auto& inst = jeecs::Tilemap::documents::inst();
    if (!doc->path.empty() && doc->path != path)
        inst.tilesets_by_path.erase(inst.canonical_key(doc->path));
    doc->path = path;
    inst.bind_tileset_path(inst.canonical_key(path), tileset);
    return true;
}
JE_API void je_tilemap_reload_tileset(const char* path)
{
    jeecs::Tilemap::documents::inst().reload_tileset(path ? path : "");
}
JE_API uint64_t je_tilemap_tileset_version(int32_t tileset)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    return doc ? doc->version : 0;
}
JE_API const char* je_tilemap_tileset_doc_path(int32_t tileset)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    return doc ? doc->path.c_str() : "";
}
JE_API void je_tilemap_tileset_set_name(int32_t tileset, const char* name)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc != nullptr && name != nullptr)
    {
        doc->name = name;
        ++doc->version;
    }
}
JE_API const char* je_tilemap_tileset_name(int32_t tileset)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    return doc ? doc->name.c_str() : "";
}

JE_API int32_t je_tilemap_add_source(int32_t tileset, const char* texture_path, int32_t tile_px)
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
JE_API bool je_tilemap_remove_source(int32_t tileset, int32_t source_idx)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || source_idx < 0 || source_idx >= (int32_t)doc->sources.size())
        return false;
    doc->sources.erase(doc->sources.begin() + source_idx);
    // 移除引用该源的瓦片/地形并重排 source 下标
    auto fix_def = [&](auto& defs)
    {
        for (size_t i = defs.size(); i > 0; --i)
        {
            auto& d = defs[i - 1];
            if (d.source == source_idx)
                defs.erase(defs.begin() + (i - 1));
            else if (d.source > source_idx)
                d.source -= 1;
        }
    };
    fix_def(doc->tiles);
    fix_def(doc->terrains);
    doc->rebuild_flat_index();
    ++doc->version;
    return true;
}
JE_API int32_t je_tilemap_source_count(int32_t tileset)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    return doc ? (int32_t)doc->sources.size() : 0;
}
JE_API bool je_tilemap_source_info(int32_t tileset, int32_t source_idx,
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

JE_API int32_t je_tilemap_add_tile(int32_t tileset, int32_t source_idx, int32_t ix, int32_t iy, int32_t walkable)
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
JE_API bool je_tilemap_remove_tile(int32_t tileset, int32_t tile_id)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || tile_id <= 0 || tile_id >= (int32_t)doc->tiles.size())
        return false;
    doc->tiles.erase(doc->tiles.begin() + tile_id);
    doc->tile_properties.erase(tile_id);
    // 图集内部 id 重排：引用该图集的地图格子需要整体修正，代价是扫描
    // 所有缓存地图（图集定义变更属于低频操作）
    for (auto& map_doc : jeecs::Tilemap::documents::inst().maps)
    {
        if (map_doc == nullptr)
            continue;
        bool changed = false;
        for (size_t ts_idx = 0; ts_idx < map_doc->resolved.size(); ++ts_idx)
        {
            if (map_doc->resolved[ts_idx].get() != doc)
                continue;
            for (auto& layer : map_doc->layers)
                for (auto& v : layer.grid)
                {
                    if (v <= 0) continue;
                    int32_t ti = 0, local = 0;
                    jeecs::Tilemap::decode_tile_value(v, &ti, &local);
                    if (ti != (int32_t)ts_idx || local < tile_id)
                        continue;
                    if (local == tile_id) { v = 0; changed = true; }
                    else { v = jeecs::Tilemap::encode_tile_value(ti, local - 1); changed = true; }
                }
        }
        if (changed)
            ++map_doc->version;
    }
    doc->rebuild_flat_index();
    ++doc->version;
    return true;
}
JE_API int32_t je_tilemap_tile_count(int32_t tileset)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    return doc ? (int32_t)doc->tiles.size() - 1 : 0;
}
JE_API bool je_tilemap_tile_info(int32_t tileset, int32_t tile_id,
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
JE_API void je_tilemap_set_tile_walkable(int32_t tileset, int32_t tile_id, int32_t walkable)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || tile_id <= 0 || tile_id >= (int32_t)doc->tiles.size())
        return;
    doc->tiles[tile_id].walkable = walkable != 0;
    ++doc->version;
}

JE_API int32_t je_tilemap_add_terrain(int32_t tileset, const char* name, int32_t kind,
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
JE_API bool je_tilemap_remove_terrain(int32_t tileset, int32_t terrain_id)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size())
        return false;
    doc->terrains.erase(doc->terrains.begin() + terrain_id);
    for (auto& map_doc : jeecs::Tilemap::documents::inst().maps)
    {
        if (map_doc == nullptr)
            continue;
        bool changed = false;
        for (size_t ts_idx = 0; ts_idx < map_doc->resolved.size(); ++ts_idx)
        {
            if (map_doc->resolved[ts_idx].get() != doc)
                continue;
            for (auto& layer : map_doc->layers)
                for (auto& v : layer.grid)
                {
                    if (v >= 0) continue;
                    int32_t ti = 0, local = 0;
                    jeecs::Tilemap::decode_tile_value(v, &ti, &local);
                    if (ti != (int32_t)ts_idx || local < terrain_id)
                        continue;
                    if (local == terrain_id) { v = 0; changed = true; }
                    else { v = jeecs::Tilemap::encode_terrain_value(ti, local - 1); changed = true; }
                }
        }
        if (changed)
            ++map_doc->version;
    }
    ++doc->version;
    return true;
}
JE_API int32_t je_tilemap_terrain_count(int32_t tileset)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    return doc ? (int32_t)doc->terrains.size() - 1 : 0;
}
JE_API bool je_tilemap_terrain_info(int32_t tileset, int32_t terrain_id,
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
JE_API bool je_tilemap_set_terrain(int32_t tileset, int32_t terrain_id,
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
    auto& t = doc->terrains[terrain_id];
    if (name != nullptr) t.name = name;
    t.kind = kind;
    t.source = source_idx;
    t.ix = ix; t.iy = iy;
    t.walkable = walkable != 0;
    ++doc->version;
    return true;
}
JE_API void je_tilemap_terrain_clear_variants(int32_t tileset, int32_t terrain_id)
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
JE_API bool je_tilemap_terrain_add_variant(int32_t tileset, int32_t terrain_id, int32_t tile_id)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size()
        || tile_id <= 0 || tile_id >= (int32_t)doc->tiles.size())
        return false;
    doc->terrains[terrain_id].variant_tiles.push_back(tile_id);
    ++doc->version;
    return true;
}

JE_API int32_t je_tilemap_add_property(int32_t tileset, const char* name, const char* type, const char* default_value)
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
JE_API bool je_tilemap_remove_property(int32_t tileset, const char* name)
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
JE_API int32_t je_tilemap_property_count(int32_t tileset)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    return doc ? (int32_t)doc->properties.size() : 0;
}
JE_API bool je_tilemap_property_info(int32_t tileset, int32_t index,
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
JE_API void je_tilemap_set_tile_property(int32_t tileset, int32_t tile_id,
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
JE_API const char* je_tilemap_get_tile_property(int32_t tileset, int32_t tile_id, const char* name)
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
JE_API void je_tilemap_set_terrain_property(int32_t tileset, int32_t terrain_id,
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
JE_API const char* je_tilemap_get_terrain_property(int32_t tileset, int32_t terrain_id, const char* name)
{
    auto* doc = je_tilemap_ts_doc(tileset);
    if (doc == nullptr || terrain_id <= 0 || terrain_id >= (int32_t)doc->terrains.size()
        || name == nullptr)
        return nullptr;
    auto fnd = doc->terrains[terrain_id].properties.find(name);
    return fnd == doc->terrains[terrain_id].properties.end() ? nullptr : fnd->second.c_str();
}

// ======================================================================
// 单元格渲染信息（编辑器预览用，与实际渲染一致）
// ======================================================================
namespace jeecs::Tilemap
{
    struct cell_quad_info
    {
        int32_t source_idx = 0;         // 图集内源纹理下标
        float u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f; // 引擎空间 UV（v0 下 v1 上）
        int32_t qx = 0, qy = 0, qcols = 1;            // 目标象限（qcols×qcols 自左上角）
    };

    // 解析某格的 1~4 个子四边形；返回数量（0 = 空格或不可解析）
    inline int32_t resolve_cell_quads(MapDocument& doc, int32_t layer,
        int32_t x, int32_t y, cell_quad_info* out /*至少 4 项*/)
    {
        if (!doc.in_bounds(layer, x, y))
            return 0;
        // API 直调时无系统代管图集解析，此处兜底（幂等）
        documents::inst().ensure_tilesets(doc);
        int32_t v = doc.layers[layer].grid[y * doc.width + x];
        if (v == 0)
            return 0;
        int32_t ts_idx = 0, local = 0;
        decode_tile_value(v, &ts_idx, &local);
        if (ts_idx < 0 || ts_idx >= (int32_t)doc.resolved.size()
            || doc.resolved[ts_idx] == nullptr)
            return 0;
        const auto* ts = doc.resolved[ts_idx].get();

        auto fill_full = [&](const SourceTexture& src, int32_t ix, int32_t iy)
        {
            out[0].source_idx = -1; // 由调用方按位置填
            float w = (float)(src.tex_w ? src.tex_w : src.xcount * src.tile_px);
            float h = (float)(src.tex_h ? src.tex_h : src.ycount * src.tile_px);
            float px = (float)(ix * src.tile_px), py = (float)(iy * src.tile_px);
            float sz = (float)src.tile_px;
            out[0].u0 = px / w;
            out[0].u1 = (px + sz) / w;
            out[0].v1 = 1.f - py / h;
            out[0].v0 = 1.f - (py + sz) / h;
            out[0].qx = 0; out[0].qy = 0; out[0].qcols = 1;
        };

        if (v > 0)
        {
            if (local <= 0 || local >= (int32_t)ts->tiles.size())
                return 0;
            const auto& t = ts->tiles[local];
            if (t.source < 0 || t.source >= (int32_t)ts->sources.size())
                return 0;
            fill_full(ts->sources[t.source], t.ix, t.iy);
            out[0].source_idx = t.source;
            return 1;
        }

        if (local <= 0 || local >= (int32_t)ts->terrains.size())
            return 0;
        const auto& terrain = ts->terrains[local];
        if (terrain.source < 0 || terrain.source >= (int32_t)ts->sources.size())
            return 0;
        const auto& src = ts->sources[terrain.source];

        if (terrain.kind == 1 || terrain.kind == 3)
        {
            // RPGMaker 四象限（XP 式条带 / VX 式 2x3 块）：
            // 槽位序 [LU, RU, LD, RD]，与渲染系统一致
            int mask = compute_mask(doc, layer, x, y, v);
            if (terrain.kind == 3 && mask == 0)
            {
                // VX 孤块：mkxp 变体47 = 锚点整块
                fill_full(src, terrain.ix, terrain.iy);
                out[0].source_idx = terrain.source;
                return 1;
            }
            const float shalf = src.tile_px * 0.5f;
            float w = (float)(src.tex_w ? src.tex_w : src.xcount * src.tile_px);
            float h = (float)(src.tex_h ? src.tex_h : src.ycount * src.tile_px);
            struct corner_bits { int o1, o2, diag; };
            const corner_bits cb[4] = {
                { NB_U, NB_L, NB_UL }, { NB_U, NB_R, NB_RU },
                { NB_D, NB_L, NB_LD }, { NB_D, NB_R, NB_RD },
            };
            const int32_t dst_qx[4] = { 0, 1, 0, 1 };
            const int32_t dst_qy[4] = { 0, 0, 1, 1 };
            for (int c = 0; c < 4; ++c)
            {
                half_cell hc = terrain.kind == 1
                    ? rpgmaker_quadrant(c, corner_state(mask, cb[c].o1, cb[c].o2, cb[c].diag))
                    : rpgmaker_vx_quadrant(c, corner_state(mask, cb[c].o1, cb[c].o2, cb[c].diag));
                float px = terrain.ix * src.tile_px + hc.col * shalf;
                float py = terrain.iy * src.tile_px + hc.row * shalf;
                out[c].source_idx = terrain.source;
                out[c].u0 = px / w;
                out[c].u1 = (px + shalf) / w;
                out[c].v1 = 1.f - py / h;
                out[c].v0 = 1.f - (py + shalf) / h;
                out[c].qx = dst_qx[c];
                out[c].qy = dst_qy[c];
                out[c].qcols = 2;
            }
            return 4;
        }

        // 单块：直接取锚点整块（不依赖瓦片定义表）
        if (terrain.kind == 0)
        {
            fill_full(src, terrain.ix, terrain.iy);
            out[0].source_idx = terrain.source;
            return 1;
        }

        // blob47：整块变体
        int32_t variant_tile = 0;
        {
            int mask = compute_mask(doc, layer, x, y, v);
            uint8_t variant = blob47_table()[(uint8_t)(mask & 0xFF)];
            if ((size_t)variant < terrain.variant_tiles.size())
                variant_tile = terrain.variant_tiles[variant];
            else
                variant_tile = ts->tile_at_flat(terrain.source,
                    terrain.iy * src.xcount + terrain.ix + variant);
        }
        if (variant_tile > 0 && variant_tile < (int32_t)ts->tiles.size())
        {
            const auto& t = ts->tiles[variant_tile];
            if (t.source >= 0 && t.source < (int32_t)ts->sources.size())
            {
                fill_full(ts->sources[t.source], t.ix, t.iy);
                out[0].source_idx = t.source;
                return 1;
            }
        }
        return 0;
    }
}

JE_API int32_t je_tilemap_cell_quad_count(int32_t map, int32_t layer, int32_t x, int32_t y)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr)
        return 0;
    jeecs::Tilemap::cell_quad_info quads[4];
    return jeecs::Tilemap::resolve_cell_quads(*doc, layer, x, y, quads);
}
JE_API bool je_tilemap_cell_quad(int32_t map, int32_t layer, int32_t x, int32_t y, int32_t idx,
    const char** texture_path,
    float* u0, float* v0, float* u1, float* v1,
    int32_t* qx, int32_t* qy, int32_t* qcols)
{
    auto* doc = je_tilemap_map_doc(map);
    if (doc == nullptr || idx < 0 || idx > 3)
        return false;
    jeecs::Tilemap::cell_quad_info quads[4];
    if (idx >= jeecs::Tilemap::resolve_cell_quads(*doc, layer, x, y, quads))
        return false;

    const auto& q = quads[idx];
    // 反查图集与源纹理
    int32_t v = doc->in_bounds(layer, x, y) ? doc->layers[layer].grid[y * doc->width + x] : 0;
    int32_t ts_idx = 0, local = 0;
    jeecs::Tilemap::decode_tile_value(v, &ts_idx, &local);
    if (ts_idx < 0 || ts_idx >= (int32_t)doc->resolved.size()
        || doc->resolved[ts_idx] == nullptr
        || q.source_idx < 0 || q.source_idx >= (int32_t)doc->resolved[ts_idx]->sources.size())
        return false;

    if (texture_path != nullptr)
        *texture_path = doc->resolved[ts_idx]->sources[q.source_idx].path.c_str();
    if (u0 != nullptr) *u0 = q.u0;
    if (v0 != nullptr) *v0 = q.v0;
    if (u1 != nullptr) *u1 = q.u1;
    if (v1 != nullptr) *v1 = q.v1;
    if (qx != nullptr) *qx = q.qx;
    if (qy != nullptr) *qy = q.qy;
    if (qcols != nullptr) *qcols = q.qcols;
    return true;
}

// ======================================================================
// wojeapi_tilemap_*：Woolang 绑定（je/tilemap.wo 经 extern 声明引用）
// ======================================================================
WOORT_API woort_api wojeapi_tilemap_open_map(void)
{
    return woort_ret_int(je_tilemap_open_map(woort_string(0)));
}
WOORT_API woort_api wojeapi_tilemap_create_map(void)
{
    return woort_ret_int(je_tilemap_create_map(
        (int32_t)woort_int(0), (int32_t)woort_int(1), (int32_t)woort_int(2)));
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
    return woort_ret_int(je_tilemap_open_tileset(woort_string(0)));
}
WOORT_API woort_api wojeapi_tilemap_create_tileset(void)
{
    return woort_ret_int(je_tilemap_create_tileset(woort_string(0)));
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
