#pragma once

// Tilemap 文档模型层：数据结构、.je4tilemap/.je4tileset 二进制读写、
// 进程内文档注册表、自动图块（RPGMaker 四象限 / blob47）邻接解析、
// 属性取值链，以及“某格渲染成哪些子四边形”的统一解析器
//（resolve_cell_quads —— 网格构建与编辑器预览共用同一实现）。
//
// 本头文件由 jeecs_core_tilemap_system.hpp 引入（同一翻译单元），不单独
// 包含；渲染系统 TilemapSystem 与 je_tilemap_* C API 见该文件。

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
#include <tuple>
#include <string>
#include <memory>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cctype>
#include <algorithm>

namespace jeecs
{
    namespace Tilemap
    {
        // ==================== 瓦片值编码 ====================
        // 网格与笔刷统一编码：0 = 空；v>0 普通瓦片；v<0 自动图块地形。
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
            int32_t source = 0; // 所属源纹理下标；-1 = 墓碑（已删除）
            int32_t ix = 0, iy = 0; // 图集网格坐标（自左上角）
            bool walkable = true;
        };
        struct TerrainDef
        {
            std::string name;
            int32_t kind = 0;   // 0=单块 1=RPGMaker四象限(XP) 2=blob47 3=RPGMaker四象限(VX)
            int32_t source = 0; // -1 = 墓碑（已删除）
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

        // ==================== 图层着色器样式 ====================
        // uniform 类型与 jegl_shader::uniform_type 的 INT..FLOAT4 对齐
        //（TEXTURE 由纹理槽表达，矩阵类暂不开放）
        enum layer_uniform_type : uint8_t
        {
            LAYER_UNIFORM_INT = 0,
            LAYER_UNIFORM_INT2,
            LAYER_UNIFORM_INT3,
            LAYER_UNIFORM_INT4,
            LAYER_UNIFORM_FLOAT = 4,
            LAYER_UNIFORM_FLOAT2,
            LAYER_UNIFORM_FLOAT3,
            LAYER_UNIFORM_FLOAT4,
        };
        inline bool is_valid_layer_uniform_type(uint8_t t)
        {
            return t <= LAYER_UNIFORM_FLOAT4;
        }

        struct LayerUniform
        {
            std::string name;
            uint8_t type = LAYER_UNIFORM_INT;
            int32_t iv[4] = {};
            float fv[4] = {};
        };
        struct LayerTexture
        {
            int32_t slot = 1;       // 槽 0 保留给图集源纹理
            std::string path;
        };
        struct LayerStyle
        {
            std::string shader_path;                    // 空 = 未指定（回落根实体/内置）
            std::vector<LayerTexture> textures;         // 附加纹理（槽 >= 1）
            std::vector<LayerUniform> uniforms;
            // 样式独立版本戳：样式变化不 bump doc->version，避免触发
            // 全网格重扫与顶点重建；渲染侧只做轻量的着色器/纹理对齐。
            // 不落盘（加载即 1）。
            uint64_t style_version = 1;
        };

        struct MapLayer
        {
            std::string name;
            bool visible = true;
            bool locked = false;    // 编辑器锁定标记（运行时忽略）
            // 该层在世界中的 z 偏移（相对地图根实体，世界单位）。
            // 不同层须错开 z 以避免深度冲突；旧文件缺省为层下标。
            float z = 0.f;
            std::vector<int32_t> grid;  // w*h 稠密网格，行优先、y 自上而下
            LayerStyle style;           // 逐层着色器样式（可空）
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
            void f32(float v) { uint32_t bits; memcpy(&bits, &v, 4); u32(bits); }
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
            float f32()
            {
                uint32_t bits = u32();
                float v; memcpy(&v, &bits, 4); return v;
            }
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

        inline bool write_buffer_to_file(const std::vector<char>& buffer, const std::string& path)
        {
            FILE* f = fopen(resolve_runtime_prefix(path).c_str(), "wb");
            if (f == nullptr)
                return false;
            bool written = fwrite(buffer.data(), 1, buffer.size(), f) == buffer.size();
            fclose(f);
            return written;
        }
        inline std::vector<char> read_buffer_from_file(const std::string& path)
        {
            jeecs_file* f = jeecs_file_open(path.c_str());
            if (f == nullptr)
                return {};
            std::vector<char> buf(f->m_file_length);
            size_t got = jeecs_file_read(buf.data(), 1, buf.size(), f);
            jeecs_file_close(f);
            if (got != buf.size())
                return {};
            return buf;
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
            // 此计数是"带属性的瓦片条目数"，与读取侧逐条 (tid, kv 数) 对应；
            // 误写成键值对总数会让读取器越过数据末尾而判定整文件损坏。
            w.u32((uint32_t)ts.tile_properties.size());
            for (const auto& [tid, kvs] : ts.tile_properties)
            {
                w.i32(tid); w.u32((uint32_t)kvs.size());
                for (const auto& [k, v] : kvs) { w.str(k); w.str(v); }
            }
            // 尾块：地形默认属性（键为地形 id）。旧版读取器按计数读完即止，
            // 多余的尾块被忽略；旧文件无此块时加载侧保持为空（与地图
            // z 偏移尾块同款做法）。
            uint32_t terrprop_count = 0;
            for (size_t i = 1; i < ts.terrains.size(); ++i)
                if (!ts.terrains[i].properties.empty())
                    ++terrprop_count;
            w.u32(terrprop_count);
            for (size_t i = 1; i < ts.terrains.size(); ++i)
            {
                const auto& kvs = ts.terrains[i].properties;
                if (kvs.empty())
                    continue;
                w.i32((int32_t)i); w.u32((uint32_t)kvs.size());
                for (const auto& [k, v] : kvs) { w.str(k); w.str(v); }
            }
            return write_buffer_to_file(w.b, path);
        }

        inline std::shared_ptr<TilesetDocument> load_tileset_from(const std::string& path)
        {
            std::vector<char> buf = read_buffer_from_file(path);
            if (buf.empty())
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
            // 尾块（可选）：地形默认属性。旧文件无此块 → 跳过；条目至少
            // 8 字节（地形 id + 键值数），总量越界视为损坏。
            if (r.o + 4 <= r.n)
            {
                uint32_t tprop_n = r.u32();
                if (tprop_n > ts->terrains.size() || r.o + (size_t)tprop_n * 8 > r.n)
                    return nullptr;
                for (uint32_t i = 0; i < tprop_n && r.ok; ++i)
                {
                    int32_t tid = r.i32();
                    uint32_t kv_n = r.u32();
                    if (tid <= 0 || tid >= (int32_t)ts->terrains.size())
                    { r.ok = false; break; }
                    for (uint32_t k = 0; k < kv_n && r.ok; ++k)
                    {
                        std::string key = r.str(), val = r.str();
                        ts->terrains[tid].properties[std::move(key)] = std::move(val);
                    }
                }
                if (!r.ok)
                    return nullptr;
            }
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
            // 尾块：每层 z 偏移（f32）。旧版读取器按计数读完即止，多余的
            // 尾块被忽略；旧文件无此块时加载侧按层下标补默认值。
            w.u32((uint32_t)m.layers.size());
            for (const auto& l : m.layers)
                w.f32(l.z);
            // 尾块：每层着色器样式（路径 + 附加纹理 + uniform 表）。
            // style_version 不落盘。
            w.u32((uint32_t)m.layers.size());
            for (const auto& l : m.layers)
            {
                const auto& st = l.style;
                w.str(st.shader_path);
                w.u32((uint32_t)st.textures.size());
                for (const auto& t : st.textures)
                {
                    w.i32(t.slot);
                    w.str(t.path);
                }
                w.u32((uint32_t)st.uniforms.size());
                for (const auto& u : st.uniforms)
                {
                    w.str(u.name);
                    w.u8(u.type);
                    for (int32_t v : u.iv) w.i32(v);
                    for (float v : u.fv) w.f32(v);
                }
            }
            return write_buffer_to_file(w.b, path);
        }

        inline std::shared_ptr<MapDocument> load_map_from(const std::string& path)
        {
            std::vector<char> buf = read_buffer_from_file(path);
            if (buf.empty())
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
            // 尾块（可选）：每层 z 偏移。旧文件无此块 → 按层下标错开，
            // 保证多层默认不发生深度冲突。
            for (size_t i = 0; i < m->layers.size(); ++i)
                m->layers[i].z = (float)i;
            if (r.o + 4 <= r.n)
            {
                uint32_t zn = r.u32();
                if (zn > (uint32_t)m->layers.size() || r.o + (size_t)zn * 4 > r.n)
                    return nullptr;
                for (uint32_t i = 0; i < zn && r.ok; ++i)
                    m->layers[i].z = r.f32();
            }
            // 尾块（可选）：每层着色器样式。槽位/uniform 类型非法视为
            // 文件损坏，整体拒绝加载。
            if (r.o + 4 <= r.n)
            {
                uint32_t sn = r.u32();
                if (sn > (uint32_t)m->layers.size())
                    return nullptr;
                for (uint32_t i = 0; i < sn && r.ok; ++i)
                {
                    auto& st = m->layers[i].style;
                    st.shader_path = r.str();
                    uint32_t tn = r.u32();
                    if (r.o + (size_t)tn * 4 > r.n) { r.ok = false; break; }
                    for (uint32_t k = 0; k < tn && r.ok; ++k)
                    {
                        int32_t slot = r.i32();
                        std::string path = r.str();
                        if (slot < 1) { r.ok = false; break; }
                        st.textures.push_back({ slot, std::move(path) });
                    }
                    uint32_t un = r.u32();
                    if (r.o + (size_t)un * 36 > r.n) { r.ok = false; break; }
                    for (uint32_t k = 0; k < un && r.ok; ++k)
                    {
                        LayerUniform u;
                        u.name = r.str();
                        u.type = r.u8();
                        for (int c = 0; c < 4; ++c) u.iv[c] = r.i32();
                        for (int c = 0; c < 4; ++c) u.fv[c] = r.f32();
                        if (!is_valid_layer_uniform_type(u.type)) { r.ok = false; break; }
                        st.uniforms.push_back(std::move(u));
                    }
                }
                if (!r.ok)
                    return nullptr;
            }
            return m;
        }

        // ==================== 文档注册表 ====================
        // 进程内全局唯一：按路径共享文档实例，编辑器与世界经由同一份
        // 数据实现"未保存修改实时同步"。文档只增不删（C API 句柄为
        // 下标 + 1，须保持稳定）；路径 -> 句柄是派生索引，保存/重载
        // 经 save_*/reload_* 集中维护。失败路径只尝试一次，避免逐帧
        // 重试造成日志刷屏；保存/重载会清除失败记录。
        class documents
        {
        public:
            static documents& inst()
            {
                static documents d;
                return d;
            }

            // ---- 地图文档 ----
            MapDocument* map(int32_t id)
            {
                return doc_at(m_maps, id);
            }
            TilesetDocument* tileset(int32_t id)
            {
                return doc_at(m_tilesets, id);
            }

            // 打开（或复用缓存）地图文档；失败返回 {0, nullptr}
            std::pair<int32_t, MapDocument*> open_map(const std::string& path)
            {
                return open_doc<MapDocument>(
                    path, m_maps, m_maps_by_path, m_maps_failed,
                    &load_map_from, "map");
            }
            std::pair<int32_t, TilesetDocument*> open_tileset(const std::string& path)
            {
                return open_doc<TilesetDocument>(
                    path, m_tilesets, m_tilesets_by_path, m_tilesets_failed,
                    &load_tileset_from, "tileset");
            }

            // 注册新建（未落盘）文档，返回句柄
            int32_t register_map(std::shared_ptr<MapDocument> doc)
            {
                return append_doc(m_maps, std::move(doc));
            }
            int32_t register_tileset(std::shared_ptr<TilesetDocument> doc)
            {
                return append_doc(m_tilesets, std::move(doc));
            }

            // 保存并登记路径：旧路径绑定若仍指向本文档则解除，随后绑定
            // 新路径（另一文档对同路径的既有绑定被顶替 —— "最后一次
            // 保存者获胜"，与编辑器另存语义一致）。
            bool save_map(int32_t id, const std::string& path)
            {
                MapDocument* doc = map(id);
                if (doc == nullptr || path.empty())
                    return false;
                if (!save_map_to(*doc, path))
                    return false;
                rebind_path(m_maps_by_path, doc->path, path, id);
                doc->path = path;
                return true;
            }
            bool save_tileset(int32_t id, const std::string& path)
            {
                TilesetDocument* doc = tileset(id);
                if (doc == nullptr || path.empty())
                    return false;
                if (!save_tileset_to(*doc, path))
                    return false;
                rebind_path(m_tilesets_by_path, doc->path, path, id);
                doc->path = path;
                return true;
            }

            // 丢弃缓存中的未保存修改：解除路径绑定，下次访问从磁盘重读
            void reload_map(const std::string& path)
            {
                detach_map(canonical_key(path));
            }
            void reload_tileset(const std::string& path)
            {
                detach_tileset(canonical_key(path));
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
                        m.resolved[i].reset();
                        continue;
                    }
                    // 与注册表当前实例保持同一份（open 命中缓存即同指针）
                    m.resolved[i] = m_tilesets[id - 1];
                }
            }

            // 地图 + 其图集的合并版本戳：任一变化（含图集实例被重载替换）
            // 都会改变。文档实例进程内永不销毁，故指针身份是可靠的
            // 实例标识；version 区分同实例内的编辑。
            uint64_t map_stamp(const MapDocument& m) const
            {
                uint64_t stamp = mix(0x9E3779B97F4A7C15ull, (uintptr_t)&m, m.version);
                for (const auto& ts : m.resolved)
                    stamp = mix(stamp, ts ? (uintptr_t)ts.get() : 0, ts ? ts->version : 0);
                return stamp;
            }

            // find_cells 的结果槽（每地图文档一份；随文档实例走，重载后
            // 新文档自然从空表开始）
            std::vector<std::pair<int32_t, int32_t>>& find_result(const MapDocument* doc)
            {
                return m_find_results[doc];
            }

        private:
            template <typename T>
            static T* doc_at(const std::vector<std::shared_ptr<T>>& docs, int32_t id)
            {
                if (id <= 0 || id > (int32_t)docs.size() || docs[id - 1] == nullptr)
                    return nullptr;
                return docs[id - 1].get();
            }

            static uint64_t mix(uint64_t h, uintptr_t identity, uint64_t version)
            {
                return (h ^ (uint64_t)identity) * 0x100000001B3ull ^ (version * 31 + 0x2545F491);
            }

            // 注册表键规范化：@/ 与 !/ 前缀解析为真实目录，反斜杠统一为
            // 正斜杠，连续斜杠折叠，Windows 下再统一小写（文件系统不分
            // 大小写）。无前缀的相对路径（浏览器工作目录形态）按运行时
            // 目录补全，与 jeecs_file_open 的解析一致。同一文件的不同
            // 形态字符串必须映射到同一文档实例，否则编辑器两侧会各自
            // 持有独立副本，后一次保存会覆盖掉前一份里的新增数据。
            std::string canonical_key(const std::string& path) const
            {
                std::string s = resolve_runtime_prefix(path);
                bool absolute = s.size() >= 2 && (s[1] == ':' || s[0] == '/');
                if (!absolute)
                    s = std::string(::jeecs_file_get_runtime_path()) + '/' + s;
                std::string key;
                key.reserve(s.size());
                for (char raw : s)
                {
                    char ch = raw == '\\' ? '/' : raw;
#ifdef _WIN32
                    ch = (char)std::tolower((unsigned char)ch);
#endif
                    if (ch == '/' && !key.empty() && key.back() == '/')
                        continue;
                    key.push_back(ch);
                }
                return key;
            }

            // 打开（或复用）文档的通用流程：
            //   1. 路径索引命中 -> 直接返回；
            //   2. 未命中时按已注册文档的自身路径反查同一文件（修复因
            //      另存/顶替绑定而失联的文档，多份命中取 version 最新者
            //      —— 否则两份副本会互相覆盖丢数据）；
            //   3. 都没有 -> 从磁盘加载（失败记录，不重试）。
            template <typename T>
            std::pair<int32_t, T*> open_doc(
                const std::string& path,
                std::vector<std::shared_ptr<T>>& docs,
                std::unordered_map<std::string, int32_t>& by_path,
                std::set<std::string>& failed,
                std::shared_ptr<T> (*load)(const std::string&),
                const char* kind)
            {
                if (path.empty())
                    return { 0, nullptr };
                std::string key = canonical_key(path);
                auto fnd = by_path.find(key);
                if (fnd != by_path.end())
                {
                    T* d = doc_at(docs, fnd->second);
                    if (d != nullptr)
                        return { fnd->second, d };
                }
                int32_t healed = 0;
                uint64_t healed_ver = 0;
                for (size_t i = 0; i < docs.size(); ++i)
                {
                    const auto& d = docs[i];
                    if (d != nullptr && !d->path.empty() && d->version >= healed_ver
                        && canonical_key(d->path) == key)
                    {
                        healed = (int32_t)i + 1;
                        healed_ver = d->version;
                    }
                }
                if (healed > 0)
                {
                    by_path[key] = healed;
                    failed.erase(key);
                    return { healed, docs[healed - 1].get() };
                }
                if (failed.count(key) != 0)
                    return { 0, nullptr };
                auto doc = load(path);
                if (doc == nullptr)
                {
                    failed.insert(key);
                    debug::logerr("Tilemap: unable to open %s file '%s'.", kind, path.c_str());
                    return { 0, nullptr };
                }
                int32_t id = append_doc(docs, doc);
                by_path[key] = id;
                failed.erase(key);
                return { id, doc.get() };
            }

            template <typename T>
            static int32_t append_doc(std::vector<std::shared_ptr<T>>& docs,
                std::shared_ptr<T> doc)
            {
                docs.push_back(std::move(doc));
                return (int32_t)docs.size();
            }

            // 保存后重绑路径：解除仍指向本文档的旧绑定，顶替新路径上的
            // 既有绑定，并清失败记录。
            void rebind_path(std::unordered_map<std::string, int32_t>& by_path,
                const std::string& old_path, const std::string& new_path, int32_t id)
            {
                if (!old_path.empty() && old_path != new_path)
                {
                    std::string old_key = canonical_key(old_path);
                    auto fnd = by_path.find(old_key);
                    if (fnd != by_path.end() && fnd->second == id)
                        by_path.erase(fnd);
                }
                by_path[canonical_key(new_path)] = id;
            }

            // 重载除清空绑定/失败记录外，还须解除旧文档与路径的关联：
            // open_* 的反查会按文档自身路径复活旧实例，若不解除，重载
            // 语义（下次打开强制从磁盘重读）会被破坏。
            void detach_map(const std::string& key)
            {
                auto fnd = m_maps_by_path.find(key);
                if (fnd != m_maps_by_path.end())
                {
                    MapDocument* d = map(fnd->second);
                    if (d != nullptr && canonical_key(d->path) == key)
                    {
                        m_find_results.erase(d);
                        d->path.clear();
                    }
                }
                m_maps_by_path.erase(key);
                m_maps_failed.erase(key);
            }
            void detach_tileset(const std::string& key)
            {
                auto fnd = m_tilesets_by_path.find(key);
                if (fnd != m_tilesets_by_path.end())
                {
                    TilesetDocument* d = tileset(fnd->second);
                    if (d != nullptr && canonical_key(d->path) == key)
                        d->path.clear();
                }
                m_tilesets_by_path.erase(key);
                m_tilesets_failed.erase(key);
            }

            std::vector<std::shared_ptr<MapDocument>> m_maps;
            std::unordered_map<std::string, int32_t> m_maps_by_path;
            std::set<std::string> m_maps_failed;

            std::vector<std::shared_ptr<TilesetDocument>> m_tilesets;
            std::unordered_map<std::string, int32_t> m_tilesets_by_path;
            std::set<std::string> m_tilesets_failed;

            std::map<const MapDocument*, std::vector<std::pair<int32_t, int32_t>>> m_find_results;
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

        // RPGMaker 式四象限取件表。XP 条带（kind 1）与 VX 2x3 块（kind 3）
        // 共用同一张 4x6 半格取件表 —— 二者仅"孤块(mask==0)"的处理不同
        //（VX 取锚点整块，XP 仍按表取样），由调用方区分。
        // 条带位于锚点 (ix,iy) 起的 2(宽)×3(高) 图集格，四象限自条带的
        // 4×6 半格网格中取样。下表为各角 5 种邻接状态到半格 (列,行) 的
        // 映射，自旧版编辑器（f8a48343^ 的 tilemap/main.wo）像素采样逻辑
        // 解码移植；与 mkxp-z autotileVXRectsA（48 变体权威表）交叉验证
        // 一致（255/255 掩码命中，47/47 行全覆盖）。
        // 状态序：[两侧+斜角, 两侧无斜角, 第一方向边, 第二方向边, 无邻接]
        //   LU/RU 的"第一方向"为 U（上下中的上）；LD/RD 为 D；第二方向 L/R。
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
        inline int corner_state(int mask, int o1, int o2, int diag)
        {
            if ((mask & o1) && (mask & o2))
                return (mask & diag) ? 0 : 1;
            if (mask & o1) return 2;
            if (mask & o2) return 3;
            return 4;
        }

        // blob47 归约：256 个 8bit 掩码 -> 47 个变体号。
        // 两个掩码映射到同一变体，当且仅当四个角的状态各自相同
        //（0凸 1边 2凹 3内）；仅当两个相邻正交位都存在时对应对角位
        // 才参与区分。枚举序 = 正交掩码 0..15 为主序、相关对角组合为
        // 次序的首次出现序（即本引擎的规范 47 变体序）。
        inline const std::vector<uint8_t>& blob47_table()
        {
            static const std::vector<uint8_t> table = []
            {
                std::vector<uint8_t> t(256, 0);
                std::map<std::tuple<int, int, int, int>, uint8_t> corner_states;
                uint8_t next = 0;
                for (int omask = 0; omask < 16; ++omask)
                {
                    bool u = omask & NB_U, d = omask & NB_D, l = omask & NB_L, r = omask & NB_R;
                    int diag_relevant = 0;
                    if (u && l) diag_relevant |= NB_UL;
                    if (d && l) diag_relevant |= NB_LD;
                    if (u && r) diag_relevant |= NB_RU;
                    if (d && r) diag_relevant |= NB_RD;
                    for (int dmask = 0; dmask < 16; ++dmask)
                    {
                        // 把 dmask 的位 0..3 映射为真实对角位 (UL,LD,RU,RD)
                        int real = 0;
                        if (dmask & 1) real |= NB_UL;
                        if (dmask & 2) real |= NB_LD;
                        if (dmask & 4) real |= NB_RU;
                        if (dmask & 8) real |= NB_RD;
                        int key_d = real & diag_relevant;
                        // 每角的 2bit 状态：0凸 1边 2凹 3内
                        auto corner = [](bool o1, bool o2, bool dg) -> int
                        {
                            if (o1 && o2) return dg ? 3 : 2;
                            if (o1 || o2) return 1;
                            return 0;
                        };
                        auto skey = std::make_tuple(
                            corner(u, l, (key_d & NB_UL) != 0),
                            corner(u, r, (key_d & NB_RU) != 0),
                            corner(d, l, (key_d & NB_LD) != 0),
                            corner(d, r, (key_d & NB_RD) != 0));
                        auto fnd = corner_states.find(skey);
                        if (fnd == corner_states.end())
                            corner_states[skey] = next++;
                        t[omask | (real & 0xF0)] = corner_states[skey];
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
            // 2. 瓦片/地形默认 -> 3. schema 默认值
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

        // ==================== 单元格渲染解析（唯一权威实现） ====================
        // 把某格解析为 1~4 个带 UV 的子四边形。网格构建（TilemapSystem）
        // 与编辑器预览（je_tilemap_cell_quad*）共用本实现，保证两者所见
        // 永远一致（含自动图块的邻接象限解析）。
        struct cell_quad_info
        {
            int32_t tileset_idx = 0;        // 地图引用表中的图集下标
            int32_t source_idx = 0;         // 图集内源纹理下标
            float u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f; // 引擎空间 UV（v0 下 v1 上）
            // 目标象限：-1 = 整格；0=LU 1=RU 2=LD 3=RD（四象限地形）
            int32_t corner = -1;
        };

        // UV 矩形（图集源纹理网格坐标 -> 引擎 UV；v0 下 v1 上）
        inline void tile_uv(const SourceTexture& src, int32_t ix, int32_t iy,
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

        // 解析某格的子四边形到 out（至少 4 项）；返回数量
        //（0 = 空格或不可解析）。
        inline int32_t resolve_cell_quads(MapDocument& doc, int32_t layer,
            int32_t x, int32_t y, cell_quad_info* out)
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

            // 整格取件：普通瓦片 / 单块地形 / blob47 变体
            auto fill_full = [&](const SourceTexture& src, int32_t ix, int32_t iy)
            {
                out[0].tileset_idx = ts_idx;
                out[0].source_idx = -1; // 由调用方按位置填
                tile_uv(src, ix, iy, out[0].u0, out[0].v0, out[0].u1, out[0].v1);
                out[0].corner = -1;
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
                // 槽位序 [LU, RU, LD, RD]
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
                for (int c = 0; c < 4; ++c)
                {
                    half_cell hc = rpgmaker_quadrant(c,
                        corner_state(mask, cb[c].o1, cb[c].o2, cb[c].diag));
                    // 半格 (col,row) 的 UV：row 自条带锚点顶部向下增长
                    float px = terrain.ix * src.tile_px + hc.col * shalf;
                    float py = terrain.iy * src.tile_px + hc.row * shalf;
                    out[c].tileset_idx = ts_idx;
                    out[c].source_idx = terrain.source;
                    out[c].u0 = px / w;
                    out[c].u1 = (px + shalf) / w;
                    out[c].v1 = 1.f - py / h;
                    out[c].v0 = 1.f - (py + shalf) / h;
                    out[c].corner = c;
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

            // blob47：按邻接掩码解析为一个整块变体（显式变体表，或自
            // 锚点起行优先连续取 47 块的默认布局）
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
}
