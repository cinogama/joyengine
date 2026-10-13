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
#include <mutex>
#include <variant>
#include <array>

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

        // ==================== 属性值 ====================
        // 属性值的类型化存储：variant 备选序与 je_PropertyType 枚举值
        // 一一对应（见下方 static_assert），读写接口按枚举严格匹配，
        // 类型不符即失败（不做隐式转换）。
        using property_value = std::variant<
            bool,                   // JE_PROPERTY_TYPE_BOOL
            std::string,            // JE_PROPERTY_TYPE_STRING
            int32_t,                // JE_PROPERTY_TYPE_INT
            float,                  // JE_PROPERTY_TYPE_FLOAT
            std::array<float, 2>,   // JE_PROPERTY_TYPE_VEC2
            std::array<float, 3>,   // JE_PROPERTY_TYPE_VEC3
            std::array<float, 4>>;  // JE_PROPERTY_TYPE_VEC4

        static_assert((size_t)JE_PROPERTY_TYPE_BOOL == 0
            && std::is_same_v<std::variant_alternative_t<0, property_value>, bool>);
        static_assert((size_t)JE_PROPERTY_TYPE_STRING == 1
            && std::is_same_v<std::variant_alternative_t<1, property_value>, std::string>);
        static_assert((size_t)JE_PROPERTY_TYPE_INT == 2
            && std::is_same_v<std::variant_alternative_t<2, property_value>, int32_t>);
        static_assert((size_t)JE_PROPERTY_TYPE_FLOAT == 3
            && std::is_same_v<std::variant_alternative_t<3, property_value>, float>);
        static_assert((size_t)JE_PROPERTY_TYPE_VEC2 == 4
            && std::is_same_v<std::variant_alternative_t<4, property_value>, std::array<float, 2>>);
        static_assert((size_t)JE_PROPERTY_TYPE_VEC3 == 5
            && std::is_same_v<std::variant_alternative_t<5, property_value>, std::array<float, 3>>);
        static_assert((size_t)JE_PROPERTY_TYPE_VEC4 == 6
            && std::is_same_v<std::variant_alternative_t<6, property_value>, std::array<float, 4>>);

        inline bool property_type_valid(int32_t t)
        {
            return t >= (int32_t)JE_PROPERTY_TYPE_BOOL
                && t <= (int32_t)JE_PROPERTY_TYPE_VEC4;
        }
        // 按类型的零值（add_property 未给 default 时使用）
        inline property_value property_zero_value(je_PropertyType t)
        {
            switch (t)
            {
            case JE_PROPERTY_TYPE_STRING:
                return property_value{ std::in_place_type<std::string>, std::string() };
            case JE_PROPERTY_TYPE_INT:
                return property_value{ std::in_place_type<int32_t>, 0 };
            case JE_PROPERTY_TYPE_FLOAT:
                return property_value{ std::in_place_type<float>, 0.f };
            case JE_PROPERTY_TYPE_VEC2:
                return property_value{ std::in_place_type<std::array<float, 2>>,
                    std::array<float, 2>{ 0.f, 0.f } };
            case JE_PROPERTY_TYPE_VEC3:
                return property_value{ std::in_place_type<std::array<float, 3>>,
                    std::array<float, 3>{ 0.f, 0.f, 0.f } };
            case JE_PROPERTY_TYPE_VEC4:
                return property_value{ std::in_place_type<std::array<float, 4>>,
                    std::array<float, 4>{ 0.f, 0.f, 0.f, 0.f } };
            case JE_PROPERTY_TYPE_BOOL:
            default:
                return property_value{ std::in_place_type<bool>, false };
            }
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
            std::map<std::string, property_value> properties;   // 地形默认属性
        };
        struct PropertyDef
        {
            std::string name;
            je_PropertyType type = JE_PROPERTY_TYPE_BOOL;
            property_value default_value;    // 与 type 一致
        };

        struct TilesetDocument
        {
            std::string name;
            std::string path;   // .je4tileset 运行时路径；未保存为空
            std::vector<SourceTexture> sources;
            std::vector<TileDef> tiles;        // tiles[0] 占位，瓦片 id = 下标（1 起）
            std::vector<TerrainDef> terrains;   // terrains[0] 占位，地形 id = 下标（1 起）
            std::vector<PropertyDef> properties;
            std::map<int32_t, std::map<std::string, property_value>> tile_properties;

            uint64_t version = 1;
            // 引用计数（keep/unload）：open/create 的每个调用方各占 1，
            // 引用它的地图文档亦各占 1。归零且已保存（路径非空）时文档被
            // 注册表逐出；未保存文档永不逐出（逐出即数据丢失）。
            // 仅在注册表互斥锁内读写（Woolang GC 终结器会在 GC worker
            // 线程上并发调用 unload，不能依赖单线程递增递减）。
            int32_t keep_count = 0;
            // 注册表槽位回引（下标 + 1，0 = 未注册/已逐出）。文档实例
            // 地址即对外指针身份，此回引仅供注册表按指针定位槽位。
            // 同上：仅在注册表互斥锁内读写。
            int32_t registry_slot = 0;
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
        // 图层样式直接持有渲染资源实例（basic::resource 为共享指针，复制
        // 仍指向同一实例）：uniform 值储存在着色器实例本身，由实例的
        // 持有方直接维护。落盘时着色器/纹理按资源句柄的来源路径记录，
        // uniform 值读自实例（JE_ 引擎内置与 TEXTURE/矩阵类除外，与
        // .je4entity 保存 Shaders 的口径一致），加载后写回新实例。
        struct LayerTexture
        {
            int32_t slot = 1;       // 槽 0 保留给图集源纹理
            basic::resource<graphic::texture> texture;
        };
        struct LayerStyle
        {
            // 空 = 未指定（分片着色器列表为空）
            std::optional<basic::resource<graphic::shader>> shader;
            std::vector<LayerTexture> textures;         // 附加纹理（槽 >= 1）
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
            // 不同层须错开 z 以避免深度冲突；缺省按层下标错开。
            float z = 0.f;
            std::vector<int32_t> grid;  // w*h 稠密网格，行优先、y 自上而下
            LayerStyle style;           // 逐层着色器样式（可空）
        };
        // 地图对单个图集的引用槽：引用路径与解析结果同槽存储，天然对齐
        struct TilesetRef
        {
            std::string path;                           // 引用的图集路径
            std::shared_ptr<TilesetDocument> resolved;  // 解析出的文档实例（可空）
            // 该槽位经注册表 keep 的图集实例（nullptr = 未持有）。地图文档
            // 为其解析到的每个图集各持有 1 个引用，换绑（重载/逐出）或
            // 自身被逐出时释放。
            TilesetDocument* kept = nullptr;
        };
        struct MapDocument
        {
            std::string path;   // .je4tilemap 运行时路径；未保存为空
            int32_t width = 0, height = 0, tile_px = 32;
            std::vector<TilesetRef> tilesets;   // 引用的图集（路径 + 解析结果）
            std::vector<MapLayer> layers;
            // 逐格属性覆盖：(layer, x + y*width) -> (name -> value)
            std::map<std::pair<int32_t, int32_t>, std::map<std::string, property_value>> cell_properties;

            uint64_t version = 1;
            // 引用计数语义同 TilesetDocument::keep_count（open/create 的
            // 调用方各占 1；使用该地图的世界各占 1）。仅在注册表互斥锁内
            // 读写（GC 终结器可能并行 unload）。
            int32_t keep_count = 0;
            // 注册表槽位回引（下标 + 1，0 = 未注册/已逐出）。文档实例
            // 地址即对外指针身份，此回引仅供注册表按指针定位槽位。
            // 仅在注册表互斥锁内读写。
            int32_t registry_slot = 0;

            bool in_bounds(int32_t layer, int32_t x, int32_t y) const
            {
                return layer >= 0 && layer < (int32_t)layers.size()
                    && x >= 0 && x < width && y >= 0 && y < height;
            }
        };

        // ==================== 二进制读写 ====================
        // 与 .je4animation 同风格的自有二进制格式（魔数 + 小端字段），
        // 由 C++ 侧统一读写，避免在内核引入 TOML 依赖。
        // 属性值为类型化数据：每值前有 1 字节类型标记（je_PropertyType）。
        constexpr uint32_t TILESET_MAGIC = 0x4A545332;     // "JTS2"
        constexpr uint32_t MAP_MAGIC = 0x4A544D32;         // "JTM2"

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

        // 属性值：1 字节类型标记 + 按类型载荷（bool→u8，int→i32，
        // float→f32，string→str，vecN→N 个 f32）
        inline void write_property_value(bin_writer& w, const property_value& v)
        {
            w.u8((uint8_t)v.index());
            switch (v.index())
            {
            case JE_PROPERTY_TYPE_BOOL:
                w.u8(std::get<bool>(v) ? 1 : 0);
                break;
            case JE_PROPERTY_TYPE_STRING:
                w.str(std::get<std::string>(v));
                break;
            case JE_PROPERTY_TYPE_INT:
                w.i32(std::get<int32_t>(v));
                break;
            case JE_PROPERTY_TYPE_FLOAT:
                w.f32(std::get<float>(v));
                break;
            case JE_PROPERTY_TYPE_VEC2:
                for (float c : std::get<std::array<float, 2>>(v)) w.f32(c);
                break;
            case JE_PROPERTY_TYPE_VEC3:
                for (float c : std::get<std::array<float, 3>>(v)) w.f32(c);
                break;
            case JE_PROPERTY_TYPE_VEC4:
                for (float c : std::get<std::array<float, 4>>(v)) w.f32(c);
                break;
            }
        }
        inline property_value read_property_value(bin_reader& r)
        {
            uint8_t tag = r.u8();
            if (!r.ok || !property_type_valid(tag))
            {
                r.ok = false;
                return property_value{};
            }
            switch (tag)
            {
            case JE_PROPERTY_TYPE_BOOL: return property_value{ r.u8() != 0 };
            case JE_PROPERTY_TYPE_STRING: return property_value{ r.str() };
            case JE_PROPERTY_TYPE_INT: return property_value{ r.i32() };
            case JE_PROPERTY_TYPE_FLOAT: return property_value{ r.f32() };
            case JE_PROPERTY_TYPE_VEC2:
            {
                std::array<float, 2> a{ r.f32(), r.f32() };
                return property_value{ std::move(a) };
            }
            case JE_PROPERTY_TYPE_VEC3:
            {
                std::array<float, 3> a{ r.f32(), r.f32(), r.f32() };
                return property_value{ std::move(a) };
            }
            case JE_PROPERTY_TYPE_VEC4:
            {
                std::array<float, 4> a{ r.f32(), r.f32(), r.f32(), r.f32() };
                return property_value{ std::move(a) };
            }
            }
            r.ok = false;
            return property_value{};
        }

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
                w.str(pd.name); w.u8((uint8_t)pd.type);
                write_property_value(w, pd.default_value);
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
                for (const auto& [k, v] : kvs) { w.str(k); write_property_value(w, v); }
            }
            // 尾块：地形默认属性（键为地形 id），置于计数数据之后（与地图
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
                for (const auto& [k, v] : kvs) { w.str(k); write_property_value(w, v); }
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
            const uint32_t magic = r.u32();
            if (magic != TILESET_MAGIC)
                return nullptr;
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
                pd.name = r.str();
                pd.type = (je_PropertyType)r.u8();
                if (!property_type_valid((int32_t)pd.type))
                { r.ok = false; break; }
                pd.default_value = read_property_value(r);
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
            auto read_kv = [&](std::map<std::string, property_value>& kvs)
            {
                std::string key = r.str();
                kvs[std::move(key)] = read_property_value(r);
            };
            uint32_t tp_n = r.u32();
            for (uint32_t i = 0; i < tp_n && r.ok; ++i)
            {
                int32_t tid = r.i32();
                uint32_t kv_n = r.u32();
                for (uint32_t k = 0; k < kv_n && r.ok; ++k)
                    read_kv(ts->tile_properties[tid]);
            }
            if (!r.ok)
                return nullptr;
            // 尾块（可选）：地形默认属性。条目至少 8 字节（地形 id + 键值
            // 数），总量越界视为损坏。
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
                        read_kv(ts->terrains[tid].properties);
                }
                if (!r.ok)
                    return nullptr;
            }
            ts->rebuild_flat_index();
            return ts;
        }

        // 资源实例的来源路径（落盘用）；内置/源码创建的资源无路径，返回空
        inline std::string graphic_resource_path(
            const basic::resource<graphic::texture>& texture)
        {
            const char* p = texture->resource()->m_handle.m_path_may_null_if_builtin;
            return p != nullptr ? std::string(p) : std::string();
        }
        inline std::string graphic_resource_path(
            const basic::resource<graphic::shader>& shader)
        {
            const char* p = shader->resource()->m_handle.m_path_may_null_if_builtin;
            return p != nullptr ? std::string(p) : std::string();
        }

        // 可落盘的 uniform：JE_ 前缀是引擎内置渲染变量，TEXTURE（采样
        // 通道，由纹理槽表达）与矩阵类不落盘——与 .je4entity 保存
        // Shaders 的口径一致
        inline bool is_saved_uniform(const jegl_shader::unifrom_variables* u)
        {
            if (u->m_name == nullptr || strncmp(u->m_name, "JE_", 3) == 0)
                return false;
            return u->m_uniform_type >= jegl_shader::uniform_type::INT
                && u->m_uniform_type <= jegl_shader::uniform_type::FLOAT4;
        }

        // 把着色器实例上的 uniform 当前值写入 w（紧凑形态：名字 + 类型
        // + 按类型 1~4 个分量）
        inline void write_style_uniforms(bin_writer& w, const graphic::shader& shad)
        {
            const auto* uniforms = shad.resource()->m_custom_uniforms;
            uint32_t count = 0;
            while (uniforms != nullptr)
            {
                if (is_saved_uniform(uniforms)) ++count;
                uniforms = uniforms->m_next;
            }
            w.u32(count);

            uniforms = shad.resource()->m_custom_uniforms;
            while (uniforms != nullptr)
            {
                if (is_saved_uniform(uniforms))
                {
                    w.str(uniforms->m_name);
                    w.u8((uint8_t)uniforms->m_uniform_type);
                    switch (uniforms->m_uniform_type)
                    {
                    case jegl_shader::uniform_type::INT:
                        w.i32(uniforms->m_value.m_int); break;
                    case jegl_shader::uniform_type::INT2:
                        w.i32(uniforms->m_value.m_int2[0]);
                        w.i32(uniforms->m_value.m_int2[1]); break;
                    case jegl_shader::uniform_type::INT3:
                        w.i32(uniforms->m_value.m_int3[0]);
                        w.i32(uniforms->m_value.m_int3[1]);
                        w.i32(uniforms->m_value.m_int3[2]); break;
                    case jegl_shader::uniform_type::INT4:
                        for (int c : uniforms->m_value.m_int4) w.i32(c);
                        break;
                    case jegl_shader::uniform_type::FLOAT:
                        w.f32(uniforms->m_value.m_float); break;
                    case jegl_shader::uniform_type::FLOAT2:
                        w.f32(uniforms->m_value.m_float2[0]);
                        w.f32(uniforms->m_value.m_float2[1]); break;
                    case jegl_shader::uniform_type::FLOAT3:
                        w.f32(uniforms->m_value.m_float3[0]);
                        w.f32(uniforms->m_value.m_float3[1]);
                        w.f32(uniforms->m_value.m_float3[2]); break;
                    case jegl_shader::uniform_type::FLOAT4:
                        for (float c : uniforms->m_value.m_float4) w.f32(c);
                        break;
                    default: break;   // is_saved_uniform 已过滤，不会到达
                    }
                }
                uniforms = uniforms->m_next;
            }
        }

        // 读回 uniform 值表并写到实例上（名字/类型与着色器声明不符时由
        // set_uniform 校验兜底：找不到名字为无操作，类型不符告警）。
        // shad 为空（着色器未指定/加载失败）时只解析不应用，保持流位置
        inline void read_style_uniforms(bin_reader& r, graphic::shader* shad)
        {
            uint32_t un = r.u32();
            if (r.o + (size_t)un * 9 > r.n) { r.ok = false; return; }
            for (uint32_t k = 0; k < un && r.ok; ++k)
            {
                std::string name = r.str();
                uint8_t type = r.u8();
                if (type > (uint8_t)jegl_shader::uniform_type::FLOAT4)
                {
                    r.ok = false;    // 类型非法视为文件损坏
                    break;
                }
                switch ((jegl_shader::uniform_type)type)
                {
                case jegl_shader::uniform_type::INT:
                {
                    int32_t v = r.i32();
                    if (r.ok && shad != nullptr) shad->set_uniform(name, v);
                    break;
                }
                case jegl_shader::uniform_type::INT2:
                {
                    int32_t a = r.i32();
                    int32_t b = r.i32();
                    if (r.ok && shad != nullptr) shad->set_uniform(name, a, b);
                    break;
                }
                case jegl_shader::uniform_type::INT3:
                {
                    int32_t a = r.i32();
                    int32_t b = r.i32();
                    int32_t c = r.i32();
                    if (r.ok && shad != nullptr) shad->set_uniform(name, a, b, c);
                    break;
                }
                case jegl_shader::uniform_type::INT4:
                {
                    int32_t a = r.i32();
                    int32_t b = r.i32();
                    int32_t c = r.i32();
                    int32_t d = r.i32();
                    if (r.ok && shad != nullptr) shad->set_uniform(name, a, b, c, d);
                    break;
                }
                case jegl_shader::uniform_type::FLOAT:
                {
                    float a = r.f32();
                    if (r.ok && shad != nullptr) shad->set_uniform(name, a);
                    break;
                }
                case jegl_shader::uniform_type::FLOAT2:
                {
                    float a = r.f32();
                    float b = r.f32();
                    if (r.ok && shad != nullptr) shad->set_uniform(name, math::vec2(a, b));
                    break;
                }
                case jegl_shader::uniform_type::FLOAT3:
                {
                    float a = r.f32();
                    float b = r.f32();
                    float c = r.f32();
                    if (r.ok && shad != nullptr) shad->set_uniform(name, math::vec3(a, b, c));
                    break;
                }
                case jegl_shader::uniform_type::FLOAT4:
                {
                    float a = r.f32();
                    float b = r.f32();
                    float c = r.f32();
                    float d = r.f32();
                    if (r.ok && shad != nullptr)
                        shad->set_uniform(name, math::vec4(a, b, c, d));
                    break;
                }
                default: break;      // type 已验证 0..7，不会到达
                }
            }
        }

        inline bool save_map_to(const MapDocument& m, const std::string& path)
        {
            bin_writer w;
            w.u32(MAP_MAGIC);
            w.i32(m.width); w.i32(m.height); w.i32(m.tile_px);
            w.u32((uint32_t)m.tilesets.size());
            for (const auto& ts : m.tilesets) w.str(ts.path);
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
                for (const auto& [k, v] : kvs) { w.str(k); write_property_value(w, v); }
            }
            // 尾块：每层 z 偏移（f32），置于计数数据之后。
            w.u32((uint32_t)m.layers.size());
            for (const auto& l : m.layers)
                w.f32(l.z);
            // 尾块：每层着色器样式（着色器路径 + 附加纹理表 + uniform 值
            // 表）。资源按其来源路径落盘：无路径的实例（内置/源码创建）
            // 告警并按未配置处理（uniform 值随之不落盘）。style_version
            // 不落盘。
            w.u32((uint32_t)m.layers.size());
            for (const auto& l : m.layers)
            {
                const auto& st = l.style;
                std::string shader_path =
                    st.shader.has_value() ? graphic_resource_path(st.shader.value())
                    : std::string();
                const bool save_shader =
                    st.shader.has_value() && !shader_path.empty();
                if (st.shader.has_value() && !save_shader)
                    debug::logerr("Tilemap: layer shader is not created from a file, "
                        "style is dropped when saving.");
                w.str(shader_path);

                std::vector<std::pair<int32_t, std::string>> tex_entries;
                for (const auto& t : st.textures)
                {
                    std::string tex_path = graphic_resource_path(t.texture);
                    if (tex_path.empty())
                    {
                        debug::logerr("Tilemap: layer texture at slot %d is not created "
                            "from a file, binding is dropped when saving.", t.slot);
                        continue;
                    }
                    tex_entries.emplace_back(t.slot, std::move(tex_path));
                }
                w.u32((uint32_t)tex_entries.size());
                for (const auto& [slot, tex_path] : tex_entries)
                {
                    w.i32(slot);
                    w.str(tex_path);
                }
                if (save_shader)
                    write_style_uniforms(w, *st.shader.value());
                else
                    w.u32(0);
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
            const uint32_t magic = r.u32();
            if (magic != MAP_MAGIC)
                return nullptr;
            m->width = r.i32(); m->height = r.i32(); m->tile_px = r.i32();
            if (m->width < 0 || m->height < 0 || m->tile_px <= 0
                || m->width > 65536 || m->height > 65536)
                return nullptr;
            uint32_t ts_n = r.u32();
            for (uint32_t i = 0; i < ts_n && r.ok; ++i)
                m->tilesets.push_back(TilesetRef{r.str(), nullptr, nullptr});
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
                    std::string key = r.str();
                    m->cell_properties[{ layer, cell }][std::move(key)] =
                        read_property_value(r);
                }
            }
            if (!r.ok)
                return nullptr;
            // 尾块（可选）：每层 z 偏移。缺块时按层下标错开，保证多层
            // 默认不发生深度冲突。
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
            // 尾块（可选）：每层着色器样式（着色器路径 + 附加纹理表 +
            // uniform 值表）。槽位/uniform 类型非法视为文件损坏，整体
            // 拒绝加载；资源加载失败只告警并按未配置处理（不拒绝文档）。
            if (r.o + 4 <= r.n)
            {
                uint32_t sn = r.u32();
                if (sn > (uint32_t)m->layers.size())
                    return nullptr;
                for (uint32_t i = 0; i < sn && r.ok; ++i)
                {
                    auto& st = m->layers[i].style;
                    std::string shader_path = r.str();
                    graphic::shader* shad = nullptr;
                    if (!shader_path.empty())
                    {
                        if (auto loaded = graphic::shader::load(nullptr, shader_path))
                        {
                            st.shader = loaded;
                            shad = st.shader.value().get();
                        }
                        else
                            debug::logerr(
                                "Tilemap: unable to load layer shader '%s'.",
                                shader_path.c_str());
                    }
                    uint32_t tn = r.u32();
                    if (r.o + (size_t)tn * 4 > r.n) { r.ok = false; break; }
                    for (uint32_t k = 0; k < tn && r.ok; ++k)
                    {
                        int32_t slot = r.i32();
                        std::string path = r.str();
                        if (slot < 1) { r.ok = false; break; }
                        if (auto tex = graphic::texture::load(nullptr, path))
                            st.textures.push_back(LayerTexture{ slot, std::move(tex.value()) });
                        else
                            debug::logerr(
                                "Tilemap: unable to load layer texture '%s'.",
                                path.c_str());
                    }
                    // uniform 值写回着色器实例（shad 为空时只解析不应用）
                    read_style_uniforms(r, shad);
                }
                if (!r.ok)
                    return nullptr;
            }
            return m;
        }

        // ==================== 文档注册表 ====================
        // 进程内全局唯一：按路径共享文档实例，编辑器与世界经由同一份
        // 数据实现"未保存修改实时同步"。对外以文档实例指针（je_Tilemap
        // _Document*/je_TilesetDocument*）标识；内部槽位只增不删（路径
        // 索引以槽位下标为值，须保持稳定）。文档引用计数（keep_count）
        // 归零且已保存时被逐出——槽位置空、实例销毁，此后旧指针失效。
        // 路径 -> 槽位是派生索引，保存/重载经 save_*/reload_* 集中维护。
        // 失败路径只尝试一次，避免逐帧重试造成日志刷屏；保存/重载会清除
        // 失败记录。
        //
        // ---- 线程安全 ----
        // 并发访问方至少有三类：编辑器所在线程（je_tilemap_* C API）、
        // 各世界的更新线程（TilemapSystem，经 parallel_foreach 可并行）、
        // Woolang GC worker 线程（gchandle 终结器并行调用 unload_*，见
        // woje_tilemap_release_*）。所有注册表结构性状态（槽位表、路径
        // 索引、keep_count/registry_slot）由 m_mutex 串行化；逐出（含
        // 级联释放图集引用与文档析构）在持锁状态下原子完成，保证
        // "归零逐出"与并发 keep/open 之间无竞态。锁为递归锁：unload ->
        // evict -> release_resolved -> unload 的级联在锁内自然重入。
        // 文档内容（网格/图层等）仍为设计上的共享可变数据，内容级读写
        // 不经本锁（与"未保存修改实时同步"的既有语义一致）。
        class documents
        {
        public:
            static documents& inst()
            {
                static documents d;
                return d;
            }

            // ---- 地图文档 ----

            // 已注册地图文档枚举（热更新等遍历用）：数量与按枚举下标
            //（0 起，非句柄）取实例。文档只增不删——计数含未保存的新建
            // 文档，也含被 reload 弃用的旧实例（其路径已清空，不再被世界
            // 使用，遍历侧替换样式无害）。count 与 at 两call之间注册表
            // 可能变化（并发打开/逐出），at 返回裸指针且不带引用，须立即
            // 使用；需要稳定集合时用 snapshot_maps。
            int32_t map_count() const
            {
                std::lock_guard lock(m_mutex);
                return (int32_t)m_maps.size();
            }
            MapDocument* map_at(int32_t index) const
            {
                std::lock_guard lock(m_mutex);
                if (index < 0 || index >= (int32_t)m_maps.size())
                    return nullptr;
                return m_maps[index].get();
            }

            // 快照枚举：返回当前全部已注册地图文档，每个各带 1 个引用
            //（调用方用毕须 unload，Woolang 侧即 gchandle 终结器路径）。
            // 在一次持锁内完成"收集 + 计引用"，避免逐个 count/at + keep
            // 之间文档被 GC 终结器逐出的竞态。
            std::vector<MapDocument*> snapshot_maps()
            {
                std::lock_guard lock(m_mutex);
                std::vector<MapDocument*> out;
                out.reserve(m_maps.size());
                for (auto& d : m_maps)
                    if (d != nullptr)
                    {
                        ++d->keep_count;
                        out.push_back(d.get());
                    }
                return out;
            }

            // 打开（或复用缓存）文档；失败返回 nullptr。成功返回的实例
            // 已带 1 个引用（在锁内完成"定位/加载 + 计引用"），调用方
            // 用毕 unload——这消除了"先返回指针、后 keep"窗口内被并发
            // 终结器逐出的竞态。
            MapDocument* open_map(const std::string& path)
            {
                std::lock_guard lock(m_mutex);
                return open_doc<MapDocument>(
                    path, m_maps, m_maps_by_path, m_maps_failed,
                    &load_map_from, "map");
            }
            TilesetDocument* open_tileset(const std::string& path)
            {
                std::lock_guard lock(m_mutex);
                return open_doc<TilesetDocument>(
                    path, m_tilesets, m_tilesets_by_path, m_tilesets_failed,
                    &load_tileset_from, "tileset");
            }

            // 注册新建（未落盘）文档，返回已带 1 个引用的实例指针
            MapDocument* register_map(std::shared_ptr<MapDocument> doc)
            {
                std::lock_guard lock(m_mutex);
                MapDocument* raw = append_doc(m_maps, std::move(doc));
                ++raw->keep_count;
                return raw;
            }
            TilesetDocument* register_tileset(std::shared_ptr<TilesetDocument> doc)
            {
                std::lock_guard lock(m_mutex);
                TilesetDocument* raw = append_doc(m_tilesets, std::move(doc));
                ++raw->keep_count;
                return raw;
            }

            // 保存并登记路径：旧路径绑定若仍指向本文档则解除，随后绑定
            // 新路径（另一文档对同路径的既有绑定被顶替 —— "最后一次
            // 保存者获胜"，与编辑器另存语义一致）。序列化与写盘在锁外
            //（内容属共享可变数据），路径索引重绑在锁内。
            bool save_map(MapDocument* doc, const std::string& path)
            {
                assert(doc != nullptr);
                if (path.empty())
                    return false;
                if (!save_map_to(*doc, path))
                    return false;
                std::lock_guard lock(m_mutex);
                rebind_path(m_maps_by_path, doc->path, path, registered_slot(m_maps, doc));
                doc->path = path;
                return true;
            }
            bool save_tileset(TilesetDocument* doc, const std::string& path)
            {
                assert(doc != nullptr);
                if (path.empty())
                    return false;
                if (!save_tileset_to(*doc, path))
                    return false;
                std::lock_guard lock(m_mutex);
                rebind_path(m_tilesets_by_path, doc->path, path, registered_slot(m_tilesets, doc));
                doc->path = path;
                return true;
            }

            // 丢弃缓存中的未保存修改：解除路径绑定，下次访问从磁盘重读
            void reload_map(const std::string& path)
            {
                std::lock_guard lock(m_mutex);
                detach_map(canonical_key(path));
            }
            void reload_tileset(const std::string& path)
            {
                std::lock_guard lock(m_mutex);
                detach_tileset(canonical_key(path));
            }

            // ---- 引用计数（keep/unload）----
            // keep：引用计数 +1，返回新计数。调用方持有的指针必然注册
            // 在案（引用计数 >= 1 期间不会被逐出），槽位不自洽即调用方
            // 传入了伪造/失效指针，按调用方 bug 断言。registry_slot 已
            // 清零（曾逐出后的双重释放）时按无操作兜底，避免越界访问。
            int32_t keep_map(MapDocument* doc)
            {
                std::lock_guard lock(m_mutex);
                if (doc->registry_slot <= 0)
                    return 0;
                registered_slot(m_maps, doc);
                return ++doc->keep_count;
            }
            int32_t keep_tileset(TilesetDocument* doc)
            {
                std::lock_guard lock(m_mutex);
                if (doc->registry_slot <= 0)
                    return 0;
                registered_slot(m_tilesets, doc);
                return ++doc->keep_count;
            }

            // unload：引用计数 -1（下限 0）；归零且已保存（路径非空）时
            // 文档被逐出（实例销毁、指针失效），未保存文档保留驻留避免
            // 数据丢失。返回剩余计数。GC 终结器可能在任意线程并发调用：
            // 减计数与逐出判定在同一持锁区间内完成，与并发 keep/open
            // 互斥；对已逐出实例（双重释放）按无操作兜底。
            int32_t unload_map(MapDocument* doc)
            {
                std::lock_guard lock(m_mutex);
                if (doc->registry_slot <= 0)
                    return 0;
                registered_slot(m_maps, doc);
                if (doc->keep_count > 0)
                    --doc->keep_count;
                int32_t remaining = doc->keep_count;
                if (remaining == 0 && !doc->path.empty())
                    evict_map(doc);
                return remaining;
            }
            int32_t unload_tileset(TilesetDocument* doc)
            {
                std::lock_guard lock(m_mutex);
                if (doc->registry_slot <= 0)
                    return 0;
                registered_slot(m_tilesets, doc);
                if (doc->keep_count > 0)
                    --doc->keep_count;
                int32_t remaining = doc->keep_count;
                if (remaining == 0 && !doc->path.empty())
                    evict_tileset(doc);
                return remaining;
            }

            // 逐出文档：释放内部持有（地图会级联释放其图集引用）、解除
            // 路径绑定并置空槽位（下标不回收）。此后该指针的一切访问均
            // 无效；下次按路径 open 会从磁盘重新加载。文档析构（含
            // resolved 共享指针与图层样式资源）发生在持锁状态下——
            // 这些析构不会再进入本注册表，无锁序倒置风险。
            void evict_map(MapDocument* doc)
            {
                std::lock_guard lock(m_mutex);
                const int32_t slot = registered_slot(m_maps, doc);
                release_resolved(*doc);
                if (!doc->path.empty())
                {
                    auto fnd = m_maps_by_path.find(canonical_key(doc->path));
                    if (fnd != m_maps_by_path.end() && fnd->second == slot)
                        m_maps_by_path.erase(fnd);
                }
                m_find_results.erase(doc);
                doc->registry_slot = 0;
                m_maps[slot - 1] = nullptr;
            }
            void evict_tileset(TilesetDocument* doc)
            {
                std::lock_guard lock(m_mutex);
                const int32_t slot = registered_slot(m_tilesets, doc);
                if (!doc->path.empty())
                {
                    auto fnd = m_tilesets_by_path.find(canonical_key(doc->path));
                    if (fnd != m_tilesets_by_path.end() && fnd->second == slot)
                        m_tilesets_by_path.erase(fnd);
                }
                doc->registry_slot = 0;
                m_tilesets[slot - 1] = nullptr;
            }

            // 保证地图文档的图集解析与注册表中的最新文档一致
            // （处理图集被重载/替换/逐出的情况）。每个成功解析的槽位
            // 经注册表持有该图集 1 个引用（kept 记录实例），
            // open_tileset 返回自带 1 个引用：实例未变时当即释放该多余
            // 引用，实例变化时旧持有释放、新引用转为槽位持有；解析失败
            // 同样释放。全程持锁（递归重入 open/unload）。
            void ensure_tilesets(MapDocument& m)
            {
                std::lock_guard lock(m_mutex);
                for (auto& ts : m.tilesets)
                {
                    TilesetDocument* live = open_tileset(ts.path);
                    if (live == nullptr)
                    {
                        if (ts.kept != nullptr)
                        {
                            unload_tileset(ts.kept);
                            ts.kept = nullptr;
                        }
                        ts.resolved.reset();
                        continue;
                    }
                    // 与注册表当前实例保持同一份（open 命中缓存即同指针）；
                    // 未变则释放 open 带来的多余引用
                    if (ts.kept == live
                        && ts.resolved != nullptr && ts.resolved.get() == live)
                    {
                        unload_tileset(live);
                        continue;
                    }
                    if (ts.kept != nullptr)
                        unload_tileset(ts.kept);
                    // open 的引用转为该槽位的持有（不再额外 keep）
                    ts.kept = live;
                    ts.resolved = m_tilesets[live->registry_slot - 1];
                }
            }

            // 释放地图文档持有的全部图集引用（自身被逐出/引用表重建时）
            void release_resolved(MapDocument& m)
            {
                std::lock_guard lock(m_mutex);
                for (auto& ts : m.tilesets)
                    if (ts.kept != nullptr)
                    {
                        unload_tileset(ts.kept);
                        ts.kept = nullptr;
                    }
            }

            // 地图引用表增删（删除项的图集引用随之释放）
            void append_tileset_entry(MapDocument& m, const std::string& path)
            {
                std::lock_guard lock(m_mutex);
                m.tilesets.push_back(TilesetRef{path, nullptr, nullptr});
            }
            void erase_tileset_entry(MapDocument& m, size_t idx)
            {
                std::lock_guard lock(m_mutex);
                if (idx >= m.tilesets.size())
                    return;
                if (m.tilesets[idx].kept != nullptr)
                    unload_tileset(m.tilesets[idx].kept);
                m.tilesets.erase(m.tilesets.begin() + idx);
            }

            // 地图 + 其图集的合并版本戳：任一变化（含图集实例被重载/
            // 逐出后重载替换）都会改变。指针身份作实例标识（逐出后新
            // 实例重用旧地址且版本恰好相同的概率可忽略）；version 区分
            // 同实例内的编辑。持锁以避免与并发 ensure_tilesets 重排
            // resolved（shared_ptr 写）竞争。
            uint64_t map_stamp(const MapDocument& m) const
            {
                std::lock_guard lock(m_mutex);
                uint64_t stamp = mix(0x9E3779B97F4A7C15ull, (uintptr_t)&m, m.version);
                for (const auto& ts : m.tilesets)
                    stamp = mix(stamp,
                        ts.resolved ? (uintptr_t)ts.resolved.get() : 0,
                        ts.resolved ? ts.resolved->version : 0);
                return stamp;
            }

            // find_cells 的结果槽（每地图文档一份；随文档实例走，重载后
            // 新文档自然从空表开始）。返回引用后调用方在持文档引用期间
            // 使用（keep_count >= 1 保证文档不被并发逐出，map 节点稳定）。
            std::vector<std::pair<int32_t, int32_t>>& find_result(const MapDocument* doc)
            {
                std::lock_guard lock(m_mutex);
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

            // 校验指针确为注册表在案文档并返回其槽位（下标 + 1）。
            // 合法持有的指针在引用计数 >= 1 期间必然在案（逐出即销毁），
            // 不自洽说明调用方传入了伪造/已失效指针，按调用方 bug 断言。
            // 调用方须持 m_mutex（读取槽位表）。
            template <typename T>
            static int32_t registered_slot(const std::vector<std::shared_ptr<T>>& docs, T* doc)
            {
                assert(doc != nullptr);
                assert(doc->registry_slot > 0
                    && doc->registry_slot <= (int32_t)docs.size()
                    && docs[doc->registry_slot - 1].get() == doc);
                return doc->registry_slot;
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
            // 每条成功路径都在返回前 +1 引用（调用方须持 m_mutex，open_
            // map/open_tileset 已加锁；ensure_tilesets 经递归锁重入）。
            // 未命中时的磁盘加载在锁内执行（打开是低频路径，换取
            // "加载 + 注册 + 计引用"的原子性）。
            template <typename T>
            T* open_doc(
                const std::string& path,
                std::vector<std::shared_ptr<T>>& docs,
                std::unordered_map<std::string, int32_t>& by_path,
                std::set<std::string>& failed,
                std::shared_ptr<T> (*load)(const std::string&),
                const char* kind)
            {
                if (path.empty())
                    return nullptr;
                std::string key = canonical_key(path);
                auto fnd = by_path.find(key);
                if (fnd != by_path.end())
                {
                    T* d = doc_at(docs, fnd->second);
                    if (d != nullptr)
                    {
                        ++d->keep_count;
                        return d;
                    }
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
                    ++docs[healed - 1]->keep_count;
                    return docs[healed - 1].get();
                }
                if (failed.count(key) != 0)
                    return nullptr;
                auto doc = load(path);
                if (doc == nullptr)
                {
                    failed.insert(key);
                    debug::logerr("Tilemap: unable to open %s file '%s'.", kind, path.c_str());
                    return nullptr;
                }
                by_path[key] = (int32_t)docs.size() + 1;
                failed.erase(key);
                T* raw = append_doc(docs, std::move(doc));
                ++raw->keep_count;
                return raw;
            }

            template <typename T>
            static T* append_doc(std::vector<std::shared_ptr<T>>& docs,
                std::shared_ptr<T> doc)
            {
                T* raw = doc.get();
                docs.push_back(std::move(doc));
                raw->registry_slot = (int32_t)docs.size();
                return raw;
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
                    MapDocument* d = doc_at(m_maps, fnd->second);
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
                    TilesetDocument* d = doc_at(m_tilesets, fnd->second);
                    if (d != nullptr && canonical_key(d->path) == key)
                        d->path.clear();
                }
                m_tilesets_by_path.erase(key);
                m_tilesets_failed.erase(key);
            }

            // 注册表全局互斥锁（递归：unload -> evict -> release_resolved
            // -> unload 级联在锁内重入）。mutable：map_count/map_at/
            // map_stamp 等只读接口亦须持锁（槽位表被并发打开/逐出改动）。
            mutable std::recursive_mutex m_mutex;

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
        inline const property_value* resolve_property(
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
                if (ts_idx >= 0 && ts_idx < (int32_t)doc->tilesets.size()
                    && doc->tilesets[ts_idx].resolved != nullptr)
                {
                    const auto* ts = doc->tilesets[ts_idx].resolved.get();
                    const std::map<std::string, property_value>* defaults = nullptr;
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
            if (ts_idx < 0 || ts_idx >= (int32_t)doc->tilesets.size()
                || doc->tilesets[ts_idx].resolved == nullptr)
                return true;
            const auto* ts = doc->tilesets[ts_idx].resolved.get();
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
            if (ts_idx < 0 || ts_idx >= (int32_t)doc.tilesets.size()
                || doc.tilesets[ts_idx].resolved == nullptr)
                return 0;
            const auto* ts = doc.tilesets[ts_idx].resolved.get();

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
