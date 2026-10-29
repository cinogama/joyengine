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
#include <optional>
#include <array>

namespace jeecs
{
    namespace Tilemap
    {
        // ==================== 瓦片值编码 ====================
        // 网格与笔刷统一编码（对 C ABI 与编辑器的交换格式）：
        // 0 = 空；v>0 按原样绘制瓦片；v<0 按瓦片的自动拼接类型绘制。
        // |v| = (tileset_idx + 1) * TILE_ID_BASE + tile_id
        //
        // 图集中的每个源纹理格都是瓦片：瓦片 id 由图集源分配（添加源时
        // 领一个基址，格 (ix,iy) 的 id = 基址 + iy * xcount + ix），删除
        // 源不会导致 id 漂移或复用。瓦片的通行性、名称、属性与自动拼接
        // 类型都是这条 id 上的稀疏元数据（无元数据即"默认可通行的普通
        // 瓦片"）。"地形"不再是与瓦片并列的另一种实体，而是带
        // je_TileKind 非 PLAIN 的瓦片；负值只表达"这次按自动拼接绘制"
        // （普通瓦片两种符号等价）。
        constexpr int32_t TILE_ID_BASE = 0x100000;

        inline int32_t encode_tile_value(int32_t tileset_idx, int32_t tile_id)
        {
            return (tileset_idx + 1) * TILE_ID_BASE + tile_id;
        }
        inline int32_t encode_auto_value(int32_t tileset_idx, int32_t tile_id)
        {
            return -encode_tile_value(tileset_idx, tile_id);
        }
        // 解码结果：tileset_idx 为地图引用表下标，local_id 为图集内
        // 瓦片 id（恒 > 0），is_auto 区分符号位（是否按自动拼接绘制）
        struct decoded_cell
        {
            int32_t tileset_idx;
            int32_t local_id;
            bool is_auto;
        };
        // 空（v == 0）返回 nullopt
        inline std::optional<decoded_cell> decode_tile_value(int32_t v)
        {
            if (v == 0)
                return std::nullopt;
            const int32_t a = v < 0 ? -v : v;
            return decoded_cell{ a / TILE_ID_BASE - 1, a % TILE_ID_BASE, v < 0 };
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
            // 瓦片 id 基址：本源的格子 (ix,iy) 对应瓦片 id
            // base_tile_id + iy * xcount + ix。添加源时分配、随文档保存，
            // 删除源后该区间永不复用（0 = 尚未分配）。
            int32_t base_tile_id = 0;
            // 纹理像素尺寸（UV 计算用）；nullopt = 未测量，按网格推算。
            // 有值时恒为正
            std::optional<int32_t> tex_w, tex_h;

            // 运行期：每层一个瓦片的数组纹理（build_source_array 构建，
            // 分片网格经 tex2darray 采样；不参与序列化）。
            // nullopt = 尚未构建（分片扫描会兜底重试）
            std::optional<basic::resource<graphic::texture_array>> array;
            // 运行期：构建失败标记（纹理不可读时只试一次，避免逐帧兜底
            // 重试刷日志；图集重载后随新实例重置）
            bool array_failed = false;

            // 附加层级纹理（如法线图集）：与主纹理共用同一瓦片网格
            //（xcount/ycount/tile 索引），按各自 pass 展开为独立数组纹理
            std::vector<struct SourceExtra> extras;
        };
        // 图集源纹理的附加层级：pass 为绑定通道 id（>= 1；主纹理固定
        // 占通道 0，与图层样式"槽 0 保留给图集源纹理"约定一致）
        struct SourceExtra
        {
            int32_t pass = 1;
            std::string path;   // 运行时路径（@/ 或 !/）
            // 纹理像素尺寸（添加时测量，cell 尺寸由此与主纹理网格推得）
            std::optional<int32_t> tex_w, tex_h;
            // 运行期：与主纹理同层号对齐的数组纹理（build_extra_array
            // 构建；不参与序列化）。nullopt = 尚未构建
            std::optional<basic::resource<graphic::texture_array>> array;
            // 运行期：构建失败标记（同 SourceTexture::array_failed）
            bool array_failed = false;
        };
        // 瓦片元数据：图集格 (source, ix, iy) 上的稀疏定义。没有条目的
        // 格子按默认值处理（可通行、普通瓦片、无属性、名称为空）。所有
        // 字段都只描述"这块瓦片本身"，自动图块（地形）也只是 kind 非
        // PLAIN 的瓦片，不再有独立的属性/通行性表。
        struct TileDef
        {
            std::string name;
            bool walkable = true;
            je_TileKind kind = JE_TILE_KIND_PLAIN;  // 自动拼接类型
            std::vector<int32_t> variant_tiles;     // blob47 显式变体表（瓦片 id）
            std::map<std::string, property_value> properties;   // 瓦片默认属性
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
            // 稀疏瓦片元数据：瓦片 id -> 定义（无条目 = 默认瓦片）
            std::map<int32_t, TileDef> tiles;
            // 下一个可分配的瓦片 id 基址（只增不减：删除源后其区间作废，
            // 已有地图按 id 引用不会漂移到别的格子）
            int32_t next_tile_id = 1;
            std::vector<PropertyDef> properties;

            uint64_t version = 1;

            // 某图集格子的瓦片 id（参数越界或源未分配 id 时返回 0）
            int32_t tile_id_at(int32_t source_idx, int32_t ix, int32_t iy) const
            {
                if (source_idx < 0 || source_idx >= (int32_t)sources.size())
                    return 0;
                const auto& s = sources[source_idx];
                if (s.base_tile_id <= 0 || ix < 0 || iy < 0
                    || ix >= s.xcount || iy >= s.ycount)
                    return 0;
                return s.base_tile_id + iy * s.xcount + ix;
            }
            // 瓦片 id -> (源下标, ix, iy)；不属于任何现存格子返回 nullopt
            std::optional<std::tuple<int32_t, int32_t, int32_t>>
                tile_cell(int32_t tile_id) const
            {
                if (tile_id <= 0)
                    return std::nullopt;
                for (size_t i = 0; i < sources.size(); ++i)
                {
                    const auto& s = sources[i];
                    if (s.base_tile_id <= 0)
                        continue;
                    const int64_t count = (int64_t)s.xcount * s.ycount;
                    if (tile_id < s.base_tile_id
                        || (int64_t)tile_id >= (int64_t)s.base_tile_id + count)
                        continue;
                    const int32_t off = tile_id - s.base_tile_id;
                    return std::make_tuple(
                        (int32_t)i, off % s.xcount, off / s.xcount);
                }
                return std::nullopt;
            }
            const TileDef* tile_def(int32_t tile_id) const
            {
                auto fnd = tiles.find(tile_id);
                return fnd == tiles.end() ? nullptr : &fnd->second;
            }
            // 取（必要时创建）瓦片元数据；调用方须保证 tile_id 属于
            // 某个现存格子
            TileDef& ensure_tile_def(int32_t tile_id)
            {
                return tiles[tile_id];
            }
            bool tile_walkable(int32_t tile_id) const
            {
                const auto* def = tile_def(tile_id);
                return def == nullptr || def->walkable;
            }
            je_TileKind tile_kind(int32_t tile_id) const
            {
                const auto* def = tile_def(tile_id);
                return def == nullptr ? JE_TILE_KIND_PLAIN : def->kind;
            }
            // 元数据是否已回到默认（默认条目可从稀疏表移除）
            static bool tile_def_is_default(const TileDef& def)
            {
                return def.name.empty() && def.walkable
                    && def.kind == JE_TILE_KIND_PLAIN
                    && def.variant_tiles.empty() && def.properties.empty();
            }
            // 元数据编辑器收尾：默认条目从稀疏表移除
            void compact_tile_def(int32_t tile_id)
            {
                auto fnd = tiles.find(tile_id);
                if (fnd != tiles.end() && tile_def_is_default(fnd->second))
                    tiles.erase(fnd);
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
            basic::resource<graphic::texture2d> texture;
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

        // 新建图层的缺省渲染队列基数：rend_queue = 基数 + 层下标。与场景
        // 实体常用的 0..N 小队列值错开一个数量级，保证地图默认不被同值
        // 队列的普通实体插入绘制序列
        constexpr int32_t LAYER_REND_QUEUE_BASE = 10000;

        struct MapLayer
        {
            std::string name;
            bool visible = true;
            bool locked = false;    // 编辑器锁定标记（运行时忽略）
            // 该层在世界中的 z 偏移（相对地图根实体，世界单位）。
            // 不同层须错开 z 以避免深度冲突；缺省按层下标错开。
            float z = 0.f;
            // 该层分片的渲染队列值（Renderer::Rendqueue）：同图各层按
            // 此值升序绘制（画家算法，层间叠放的权威顺序）；z 只在与
            // 写深度的场景物体之间参与裁决。缺省按基数 + 层下标错开。
            int32_t rend_queue = LAYER_REND_QUEUE_BASE;
            // 该层的 2D 光照自阴影行为（je_LayerSelfShadow）：OFF = 该层
            // 分片不挂 Light2D::SelfShadow 组件；ON/AUTO = 挂载且
            // auto_disable 分别固定为 false/true。分片组件由
            // TilemapSystem 逐帧对齐（增删走命令缓冲）。
            je_LayerSelfShadow selfshadow = JE_LAYER_SELFSHADOW_OFF;
            // 自阴影覆盖强度（写入分片 SelfShadow::factor，即覆盖色
            // RGBA 乘子，0..1 常用）：仅 selfshadow != OFF 时生效
            float selfshadow_factor = 1.f;
            // 该层的 2D 光照遮挡阴影行为（je_LayerBlockShadow）：OFF =
            // 该层分片不挂 Light2D::BlockShadow 组件；ON/AUTO = 挂载且
            // auto_disable 分别固定 false/true，factor 取 blockshadow_factor。
            // 遮挡轮廓（实心格边界，贪心合并）由 TilemapSystem 按分块
            // 网格生成到组件的 m_block_mesh（随版本戳重建）。
            je_LayerBlockShadow blockshadow = JE_LAYER_BLOCKSHADOW_OFF;
            // 遮挡阴影强度（写入分片 BlockShadow::factor，阴影体浓度，
            // 0..1 常用）：仅 blockshadow != OFF 时生效
            float blockshadow_factor = 1.f;
            std::vector<int32_t> grid;  // w*h 稠密网格，行优先、y 自上而下
            LayerStyle style;           // 逐层着色器样式（可空）
        };
        // 地图对单个图集的引用槽：引用路径与解析结果同槽存储，天然对齐。
        // 生命周期即 resolved 本身：持有 shared_ptr 即保活图集实例，地图
        // 文档析构或重新解析时随槽位释放——无任何需要手工归还的记账。
        struct TilesetRef
        {
            std::string path;   // 引用的图集路径
            // 解析出的文档实例；nullopt = 未解析（引用未保存/加载失败）。
            // 有值时内层 shared_ptr 恒非空（由解析侧保证）
            std::optional<std::shared_ptr<TilesetDocument>> resolved;
        };
        struct MapDocument
        {
            std::string path;   // .je4tilemap 运行时路径；未保存为空
            int32_t width = 0, height = 0, tile_px = 32;
            std::vector<TilesetRef> tilesets;   // 引用的图集（路径 + 解析结果）
            std::vector<MapLayer> layers;
            // 逐格属性覆盖：(layer, x + y*width) -> (name -> value)
            std::map<std::pair<int32_t, int32_t>, std::map<std::string, property_value>> cell_properties;
            // je_tilemap_find_cells 的结果缓存（find_get 逐个取回），随
            // 文档实例生死，重载/销毁后自然从空表开始
            std::vector<std::pair<int32_t, int32_t>> find_result;

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
        // 属性值为类型化数据：每值前有 1 字节类型标记（je_PropertyType）。
        // 图集（JTS5）不再兼容旧版：每个源记录自己的瓦片 id 基址，瓦片
        // 元数据按 id 稀疏保存（无条目 = 默认可通行普通瓦片）。源的
        // tex_w/tex_h 写 0 表示未测量；每个源在基础字段后带附加层级
        // 纹理表（u32 计数 + 每条 pass/path/tex_w/tex_h），pass < 1 视为
        // 损坏。瓦片 kind 为 je_TileKind 的 u8，越界视为损坏。
        // 地图（JTM6）图层记录内联 rend_queue（i32）、自阴影行为
        //（je_LayerSelfShadow 的 u8，越界视为损坏）、自阴影强度（f32）、
        // 遮挡阴影行为（je_LayerBlockShadow 的 u8，越界视为损坏）与
        // 遮挡阴影强度（f32）。瓦片值语义见文件开头的编码说明。
        constexpr uint32_t TILESET_MAGIC = 0x4A545335;     // "JTS5"
        constexpr uint32_t MAP_MAGIC = 0x4A544D36;         // "JTM6"

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
            std::vector<char> buf(jeecs_file_size(f));
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
            w.i32(ts.next_tile_id);
            w.u32((uint32_t)ts.sources.size());
            for (const auto& s : ts.sources)
            {
                w.str(s.path); w.i32(s.tile_px); w.i32(s.xcount); w.i32(s.ycount);
                w.i32(s.tex_w.value_or(0)); w.i32(s.tex_h.value_or(0));
                w.i32(s.base_tile_id);
                // 附加层级纹理表：每条 pass/path/tex_w/tex_h
                w.u32((uint32_t)s.extras.size());
                for (const auto& e : s.extras)
                {
                    w.i32(e.pass); w.str(e.path);
                    w.i32(e.tex_w.value_or(0)); w.i32(e.tex_h.value_or(0));
                }
            }
            w.u32((uint32_t)ts.properties.size());
            for (const auto& pd : ts.properties)
            {
                w.str(pd.name); w.u8((uint8_t)pd.type);
                write_property_value(w, pd.default_value);
            }
            // 稀疏瓦片元数据：只写显式定义过的瓦片（无条目 = 默认瓦片）
            w.u32((uint32_t)ts.tiles.size());
            for (const auto& [tile_id, t] : ts.tiles)
            {
                w.i32(tile_id);
                w.str(t.name);
                w.u8(t.walkable ? 1 : 0);
                w.u8((uint8_t)t.kind);
                w.u32((uint32_t)t.variant_tiles.size());
                for (int32_t v : t.variant_tiles) w.i32(v);
                w.u32((uint32_t)t.properties.size());
                for (const auto& [k, v] : t.properties)
                {
                    w.str(k); write_property_value(w, v);
                }
            }
            return write_buffer_to_file(w.b, path);
        }

        inline std::optional<std::shared_ptr<TilesetDocument>> load_tileset_from(const std::string& path)
        {
            std::vector<char> buf = read_buffer_from_file(path);
            if (buf.empty())
                return std::nullopt;

            bin_reader r{ buf.data(), buf.size(), 0, true };
            auto ts = std::make_shared<TilesetDocument>();
            ts->path = path;
            const uint32_t magic = r.u32();
            if (magic != TILESET_MAGIC)
                return std::nullopt;
            ts->name = r.str();
            ts->next_tile_id = r.i32();
            if (ts->next_tile_id <= 0 || ts->next_tile_id > TILE_ID_BASE)
                return std::nullopt;
            uint32_t src_n = r.u32();
            for (uint32_t i = 0; i < src_n && r.ok; ++i)
            {
                SourceTexture s;
                s.path = r.str();
                s.tile_px = r.i32(); s.xcount = r.i32(); s.ycount = r.i32();
                int32_t tw = r.i32(), th = r.i32();
                if (tw > 0) s.tex_w = tw;
                if (th > 0) s.tex_h = th;
                s.base_tile_id = r.i32();
                if (s.tile_px <= 0 || s.xcount <= 0 || s.ycount <= 0
                    || s.base_tile_id <= 0)
                { r.ok = false; break; }
                // 附加层级纹理表（pass 越界或同源重复视为损坏）
                uint32_t ex_n = r.u32();
                for (uint32_t k = 0; k < ex_n && r.ok; ++k)
                {
                    SourceExtra e;
                    e.pass = r.i32();
                    e.path = r.str();
                    int32_t ew = r.i32(), eh = r.i32();
                    if (ew > 0) e.tex_w = ew;
                    if (eh > 0) e.tex_h = eh;
                    if (e.pass < 1 || e.path.empty())
                    { r.ok = false; break; }
                    for (const auto& prev : s.extras)
                        if (prev.pass == e.pass)
                        { r.ok = false; break; }
                    if (r.ok)
                        s.extras.push_back(std::move(e));
                }
                if (!r.ok)
                    break;
                // 基址区间不得重叠，且须落在 next_tile_id 之前；瓦片 id
                // 还要能编入 32 位瓦片值（低 TILE_ID_BASE 位）
                const int64_t end = (int64_t)s.base_tile_id
                    + (int64_t)s.xcount * s.ycount;
                if (end > ts->next_tile_id || end > (int64_t)TILE_ID_BASE)
                { r.ok = false; break; }
                for (const auto& prev : ts->sources)
                {
                    const int64_t prev_end = (int64_t)prev.base_tile_id
                        + (int64_t)prev.xcount * prev.ycount;
                    if (s.base_tile_id < prev_end && prev.base_tile_id < end)
                    { r.ok = false; break; }
                }
                if (!r.ok)
                    break;
                ts->sources.push_back(std::move(s));
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
            uint32_t tile_n = r.u32();
            for (uint32_t i = 0; i < tile_n && r.ok; ++i)
            {
                const int32_t tile_id = r.i32();
                TileDef t;
                t.name = r.str();
                t.walkable = r.u8() != 0;
                uint8_t kind = r.u8();
                if (!ts->tile_cell(tile_id).has_value()
                    || kind > (uint8_t)JE_TILE_KIND_RPGMAKER_VX)
                { r.ok = false; break; }
                t.kind = (je_TileKind)kind;
                uint32_t vn = r.u32();
                for (uint32_t k = 0; k < vn && r.ok; ++k)
                {
                    const int32_t variant = r.i32();
                    if (!ts->tile_cell(variant).has_value())
                    { r.ok = false; break; }
                    t.variant_tiles.push_back(variant);
                }
                if (!r.ok)
                    break;
                uint32_t pn = r.u32();
                for (uint32_t k = 0; k < pn && r.ok; ++k)
                {
                    std::string key = r.str();
                    t.properties[std::move(key)] = read_property_value(r);
                }
                if (!r.ok)
                    break;
                ts->tiles[tile_id] = std::move(t);
            }
            if (!r.ok)
                return std::nullopt;
            return ts;
        }

        // 资源实例的来源路径（落盘用）；内置/源码创建的资源无路径，返回空
        inline std::string graphic_resource_path(
            const basic::resource<graphic::texture2d>& texture)
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
                default: break;      // 类型已在读入时校验
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
                w.i32(l.rend_queue); w.u8((uint8_t)l.selfshadow);
                w.f32(l.selfshadow_factor);
                w.u8((uint8_t)l.blockshadow); w.f32(l.blockshadow_factor);
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

        inline std::optional<std::shared_ptr<MapDocument>> load_map_from(const std::string& path)
        {
            std::vector<char> buf = read_buffer_from_file(path);
            if (buf.empty())
                return std::nullopt;

            bin_reader r{ buf.data(), buf.size(), 0, true };
            auto m = std::make_shared<MapDocument>();
            m->path = path;
            const uint32_t magic = r.u32();
            if (magic != MAP_MAGIC)
                return std::nullopt;
            m->width = r.i32(); m->height = r.i32(); m->tile_px = r.i32();
            if (m->width < 0 || m->height < 0 || m->tile_px <= 0
                || m->width > 65536 || m->height > 65536)
                return std::nullopt;
            uint32_t ts_n = r.u32();
            for (uint32_t i = 0; i < ts_n && r.ok; ++i)
                m->tilesets.push_back(TilesetRef{ r.str(), std::nullopt });
            uint32_t layer_n = r.u32();
            for (uint32_t i = 0; i < layer_n && r.ok; ++i)
            {
                MapLayer l;
                l.name = r.str();
                l.visible = r.u8() != 0;
                l.locked = r.u8() != 0;
                l.rend_queue = r.i32();
                uint8_t selfshadow = r.u8();
                if (selfshadow > (uint8_t)JE_LAYER_SELFSHADOW_AUTO)
                { r.ok = false; break; }
                l.selfshadow = (je_LayerSelfShadow)selfshadow;
                l.selfshadow_factor = r.f32();
                uint8_t blockshadow = r.u8();
                if (blockshadow > (uint8_t)JE_LAYER_BLOCKSHADOW_AUTO)
                { r.ok = false; break; }
                l.blockshadow = (je_LayerBlockShadow)blockshadow;
                l.blockshadow_factor = r.f32();
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
                return std::nullopt;
            // 尾块（可选）：每层 z 偏移。缺块时按层下标沿 -z 错开（与
            // 新建层默认一致，越上层 z 越小），保证多层默认不发生深度
            // 冲突。
            for (size_t i = 0; i < m->layers.size(); ++i)
                m->layers[i].z = -(float)i;
            if (r.o + 4 <= r.n)
            {
                uint32_t zn = r.u32();
                if (zn > (uint32_t)m->layers.size() || r.o + (size_t)zn * 4 > r.n)
                    return std::nullopt;
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
                    return std::nullopt;
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
                        if (auto tex = graphic::texture2d::load(nullptr, path))
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
                    return std::nullopt;
            }
            return m;
        }

        // ==================== 文档注册表 ====================
        // 进程内全局唯一：按路径共享文档实例，编辑器与世界经由同一份
        // 数据实现"未保存修改实时同步"。对外以文档实例指针（je_Tilemap
        // _Document*/je_TilesetDocument*）标识，指针身份即文档身份。
        //
        // ---- 所有权模型 ----
        // 文档实例的存活完全由 shared_ptr 决定：外部（C ABI）引用由
        // 注册表条目代为强持有（refs > 0 期间）；引用方（地图的
        // TilesetRef::resolved、TilemapSystem 经 observe 取得的句柄）
        // 各自直接持有。注册表自身只经 weak_ptr 观察实例，不为自己
        // 缓存目的保活任何文档。外部引用归零且无引用方时实例销毁
        //（含未保存文档——unload 即丢弃），下次按路径 open 从磁盘重
        // 读；地图销毁会级联释放其图集持有（RAII，无手工逐出）。
        // 路径 -> 实例是派生索引：保存/重载经 save_*/reload_* 集中
        // 维护，实例销毁后 weak 引用自然过期，解析时按需清除。失败
        // 路径只尝试一次，避免逐帧重试造成日志刷屏；保存/重载会清除
        // 失败记录。
        //
        // ---- 线程安全 ----
        // 并发访问方至少有三类：编辑器所在线程（je_tilemap_* C API）、
        // 各世界的更新线程（TilemapSystem，经 parallel_foreach 可并行）、
        // Woolang GC worker 线程（gchandle 终结器并行调用 unload_*，见
        // woje_tilemap_release_*）。所有注册表结构性状态（引用账目、
        // 路径索引、枚举表、失败集合）由 m_mutex 串行化；"解析 + 计
        // 引用"在同一锁定区间内完成，与并发 unload 之间无竞态。文档
        // 析构只触碰自身数据、不重入注册表，普通互斥锁即可。文档内容
        //（网格/图层等）仍为设计上的共享可变数据，内容级读写不经本锁
        //（与"未保存修改实时同步"的既有语义一致）。
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
            //（0 起，非句柄）取实例。枚举表只增不删，实例销毁后该槽位
            // 解析为 nullopt。count 与 at 两 call 之间注册表可能变化
            //（并发打开/销毁）；at 返回的持有在调用期间保活实例，用毕
            // 即弃、不占外部引用。需要稳定集合时用 snapshot_maps。
            int32_t map_count() const
            {
                std::lock_guard lock(m_mutex);
                return (int32_t)m_maps.all.size();
            }
            std::optional<std::shared_ptr<MapDocument>> map_at(int32_t index) const
            {
                std::lock_guard lock(m_mutex);
                if (index < 0 || index >= (int32_t)m_maps.all.size())
                    return std::nullopt;
                return live(m_maps.all[(size_t)index]);
            }

            // 快照枚举：返回当前全部存活地图文档，每个各带 1 个外部
            // 引用（调用方用毕须 unload，Woolang 侧即 gchandle 终结器
            // 路径）。在一次持锁内完成"收集 + 计引用"，避免逐个
            // count/at + keep 之间文档被并发释放的竞态。
            std::vector<MapDocument*> snapshot_maps()
            {
                std::lock_guard lock(m_mutex);
                std::vector<MapDocument*> out;
                out.reserve(m_maps.all.size());
                for (const auto& weak : m_maps.all)
                    if (auto doc = live(weak))
                    {
                        acquire_ref(m_maps, *doc);
                        out.push_back(doc->get());
                    }
                return out;
            }

            // 打开（或复用缓存）文档；失败返回 nullopt。成功返回的实例
            // 已带 1 个外部引用（在锁内完成"解析 + 计引用"），调用方
            // 用毕 unload——这消除了"先返回指针、后 keep"窗口内被并发
            // 终结器释放的竞态。
            std::optional<std::shared_ptr<MapDocument>> open_map(const std::string& path)
            {
                std::lock_guard lock(m_mutex);
                auto doc = resolve(m_maps, path, &load_map_from, "map");
                if (doc.has_value())
                    acquire_ref(m_maps, *doc);
                return doc;
            }
            std::optional<std::shared_ptr<TilesetDocument>> open_tileset(const std::string& path)
            {
                std::lock_guard lock(m_mutex);
                auto doc = resolve(m_tilesets, path, &load_tileset_from, "tileset");
                if (doc.has_value())
                    acquire_ref(m_tilesets, *doc);
                return doc;
            }

            // 观察：与 open 同一解析逻辑，但返回的持有不占外部引用计数
            //——供渲染系统等以 RAII 方式保活（析构自动让出），不参与
            // C ABI 的 keep/unload 记账。
            std::optional<std::shared_ptr<MapDocument>> observe_map(const std::string& path)
            {
                std::lock_guard lock(m_mutex);
                return resolve(m_maps, path, &load_map_from, "map");
            }
            std::optional<std::shared_ptr<TilesetDocument>> observe_tileset(const std::string& path)
            {
                std::lock_guard lock(m_mutex);
                return resolve(m_tilesets, path, &load_tileset_from, "tileset");
            }

            // 注册新建（未落盘）文档，返回已带 1 个外部引用的实例指针
            MapDocument* register_map(std::shared_ptr<MapDocument> doc)
            {
                assert(doc != nullptr);
                std::lock_guard lock(m_mutex);
                MapDocument* raw = doc.get();
                m_maps.all.push_back(doc);
                auto& entry = m_maps.held[raw];
                entry.hold = std::move(doc);
                entry.refs = 1;
                return raw;
            }
            TilesetDocument* register_tileset(std::shared_ptr<TilesetDocument> doc)
            {
                assert(doc != nullptr);
                std::lock_guard lock(m_mutex);
                TilesetDocument* raw = doc.get();
                m_tilesets.all.push_back(doc);
                auto& entry = m_tilesets.held[raw];
                entry.hold = std::move(doc);
                entry.refs = 1;
                return raw;
            }

            // 保存并登记路径：旧路径绑定若仍指向本文档则解除，随后绑定
            // 新路径（另一文档对同路径的既有绑定被顶替 —— "最后一次
            // 保存者获胜"，与编辑器另存语义一致）。序列化与写盘在锁外
            //（内容属共享可变数据），路径索引重绑在锁内。调用方须持有
            // 外部引用（无账目即契约外使用，按失败返回）。
            bool save_map(MapDocument* doc, const std::string& path)
            {
                assert(doc != nullptr);
                if (path.empty())
                    return false;
                if (!save_map_to(*doc, path))
                    return false;
                std::lock_guard lock(m_mutex);
                auto fnd = m_maps.held.find(doc);
                if (fnd == m_maps.held.end())
                    return false;
                rebind_path(m_maps, doc->path, path, fnd->second.hold);
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
                auto fnd = m_tilesets.held.find(doc);
                if (fnd == m_tilesets.held.end())
                    return false;
                rebind_path(m_tilesets, doc->path, path, fnd->second.hold);
                doc->path = path;
                return true;
            }

            // 丢弃缓存中的未保存修改：解除路径绑定，下次访问从磁盘重读。
            // 仍被引用的既有实例不受影响（其路径被清空后不再能按路径
            // 复活），随最后一个引用方销毁。
            void reload_map(const std::string& path)
            {
                std::lock_guard lock(m_mutex);
                detach(m_maps, canonical_key(path));
            }
            void reload_tileset(const std::string& path)
            {
                std::lock_guard lock(m_mutex);
                detach(m_tilesets, canonical_key(path));
            }

            // ---- 外部引用计数（keep/unload）----
            // keep：外部引用 +1，返回新计数。调用方持有的指针必然有
            // 引用账目（refs >= 1 期间实例不会被销毁），无账目即调用方
            // 传入了伪造/已失效指针，按无操作兜底返回 0。
            int32_t keep_map(MapDocument* doc)
            {
                if (doc == nullptr)
                    return 0;
                std::lock_guard lock(m_mutex);
                auto fnd = m_maps.held.find(doc);
                return fnd == m_maps.held.end() ? 0 : ++fnd->second.refs;
            }
            int32_t keep_tileset(TilesetDocument* doc)
            {
                if (doc == nullptr)
                    return 0;
                std::lock_guard lock(m_mutex);
                auto fnd = m_tilesets.held.find(doc);
                return fnd == m_tilesets.held.end() ? 0 : ++fnd->second.refs;
            }

            // unload：外部引用 -1（下限 0）；归零即撤销注册表的强持有
            //——已无引用方时实例就地销毁（此后指针失效），下次按路径
            // open 从磁盘重读。返回剩余计数。GC 终结器可能在任意线程
            // 并发调用：减计数与销毁在同一持锁区间内完成，与并发 keep/
            // open 互斥；对已销毁实例（双重释放）按无操作兜底。
            int32_t unload_map(MapDocument* doc)
            {
                if (doc == nullptr)
                    return 0;
                std::lock_guard lock(m_mutex);
                return release_ref(m_maps, doc);
            }
            int32_t unload_tileset(TilesetDocument* doc)
            {
                if (doc == nullptr)
                    return 0;
                std::lock_guard lock(m_mutex);
                return release_ref(m_tilesets, doc);
            }

            // 保证地图文档的图集解析与注册表中的最新实例一致（处理图集
            // 被重载/替换/销毁的情况）：逐槽把 resolved 重新指向当前解
            // 析结果。持有即保活——同实例赋值为无操作，实例变化/解析
            // 失败时旧实例随赋值自动释放，无任何手工换绑记账。
            void ensure_tilesets(MapDocument& m)
            {
                std::lock_guard lock(m_mutex);
                for (auto& ts : m.tilesets)
                    ts.resolved = resolve(m_tilesets, ts.path,
                        &load_tileset_from, "tileset");
            }

            // 地图引用表增删（删除项的图集持有随之自动释放）
            void append_tileset_entry(MapDocument& m, const std::string& path)
            {
                std::lock_guard lock(m_mutex);
                m.tilesets.push_back(TilesetRef{ path, std::nullopt });
            }
            void erase_tileset_entry(MapDocument& m, size_t idx)
            {
                std::lock_guard lock(m_mutex);
                if (idx >= m.tilesets.size())
                    return;
                m.tilesets.erase(m.tilesets.begin() + idx);
            }

            // 地图 + 其图集的合并版本戳：任一变化（含图集实例被重载/
            // 销毁后重载替换）都会改变。指针身份作实例标识（新实例重用
            // 旧地址且版本恰好相同的概率可忽略）；version 区分同实例内
            // 的编辑。持锁以避免与并发 ensure_tilesets 重排 resolved
            //（optional<shared_ptr> 写）竞争。
            uint64_t map_stamp(const MapDocument& m) const
            {
                std::lock_guard lock(m_mutex);
                uint64_t stamp = mix(0x9E3779B97F4A7C15ull, (uintptr_t)&m, m.version);
                for (const auto& ts : m.tilesets)
                    stamp = mix(stamp,
                        ts.resolved ? (uintptr_t)ts.resolved->get() : 0,
                        ts.resolved ? (*ts.resolved)->version : 0);
                return stamp;
            }

        private:
            // 每类文档一张表：
            //   held    —— 外部引用账目：refs > 0 期间强持有实例（代表
            //             全部 C ABI 裸指针持有者），归零即移除条目，
            //             实例是否销毁取决于是否仍有引用方持有；
            //   by_path —— 规范路径 -> 实例的弱引用索引（自愈：实例
            //             销毁后过期，解析时按需清除/重绑）；
            //   failed  —— 打开失败的路径集合（只尝试一次）；
            //   all     —— 枚举表（只增不删，保持稳定下标语义，实例
            //             销毁后槽位过期）。
            template <typename T>
            struct doc_table
            {
                struct entry
                {
                    std::shared_ptr<T> hold;  // 条目存在期间恒非空
                    int32_t refs = 0;
                };
                std::unordered_map<T*, entry> held;
                std::unordered_map<std::string, std::weak_ptr<T>> by_path;
                std::set<std::string> failed;
                std::vector<std::weak_ptr<T>> all;
            };

            // weak -> "存活实例或无"（过期的 weak 归一为 nullopt）
            template <typename T>
            static std::optional<std::shared_ptr<T>> live(const std::weak_ptr<T>& weak)
            {
                auto doc = weak.lock();
                if (doc != nullptr)
                    return doc;
                return std::nullopt;
            }

            // 记 1 个外部引用（open/snapshot 的"解析 + 计引用"收尾）
            template <typename T>
            static void acquire_ref(doc_table<T>& table, const std::shared_ptr<T>& doc)
            {
                auto& entry = table.held[doc.get()];
                if (++entry.refs == 1)
                    entry.hold = doc;
            }

            // 还 1 个外部引用；归零撤销强持有（可能就地销毁实例）
            template <typename T>
            static int32_t release_ref(doc_table<T>& table, T* doc)
            {
                auto fnd = table.held.find(doc);
                if (fnd == table.held.end())
                    return 0;
                if (fnd->second.refs > 0)
                    --fnd->second.refs;
                const int32_t remaining = fnd->second.refs;
                if (remaining == 0)
                    table.held.erase(fnd);
                return remaining;
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

            // 解析（或复用）文档的通用流程（调用方须持 m_mutex）：
            //   1. 路径索引命中且实例存活 -> 直接返回；weak 引用已过期
            //      则视为未命中并清除该条目（实例销毁后的自愈）；
            //   2. 未命中时按存活文档的自身路径反查同一文件（修复因
            //      另存/顶替绑定而失联的文档，多份命中取 version 最新
            //      者——否则两份副本会互相覆盖丢数据）；
            //   3. 都没有 -> 从磁盘加载（失败记录，不重试）。
            // 磁盘加载在锁内执行（解析是低频路径，换取"加载 + 入表 +
            // 计引用"的原子性）。本函数不计外部引用（open 在返回前
            // 另行计得，observe 则完全不计）。
            template <typename T>
            std::optional<std::shared_ptr<T>> resolve(
                doc_table<T>& table, const std::string& path,
                std::optional<std::shared_ptr<T>> (*load)(const std::string&),
                const char* kind)
            {
                if (path.empty())
                    return std::nullopt;
                const std::string key = canonical_key(path);
                auto fnd = table.by_path.find(key);
                if (fnd != table.by_path.end())
                {
                    if (auto doc = live(fnd->second))
                        return doc;
                    table.by_path.erase(fnd);   // 实例已销毁，索引自愈
                }
                std::optional<std::shared_ptr<T>> healed;
                uint64_t healed_ver = 0;
                for (const auto& weak : table.all)
                {
                    auto doc = live(weak);
                    if (!doc.has_value() || (*doc)->path.empty())
                        continue;
                    if ((*doc)->version >= healed_ver
                        && canonical_key((*doc)->path) == key)
                    {
                        healed = std::move(doc);
                        healed_ver = (*healed)->version;
                    }
                }
                if (healed.has_value())
                {
                    table.by_path[key] = *healed;
                    table.failed.erase(key);
                    return healed;
                }
                if (table.failed.count(key) != 0)
                    return std::nullopt;
                auto doc = load(path);
                if (!doc.has_value())
                {
                    table.failed.insert(key);
                    debug::logerr("Tilemap: unable to open %s file '%s'.", kind, path.c_str());
                    return std::nullopt;
                }
                table.failed.erase(key);
                table.by_path[key] = *doc;
                table.all.push_back(*doc);
                return doc;
            }

            // 保存后重绑路径：解除仍指向本文档的旧绑定，顶替新路径上的
            // 既有绑定，并清失败记录。
            template <typename T>
            void rebind_path(doc_table<T>& table, const std::string& old_path,
                const std::string& new_path, const std::shared_ptr<T>& self)
            {
                if (!old_path.empty() && old_path != new_path)
                {
                    const std::string old_key = canonical_key(old_path);
                    auto fnd = table.by_path.find(old_key);
                    if (fnd != table.by_path.end()
                        && fnd->second.lock() == self)
                        table.by_path.erase(fnd);
                }
                const std::string new_key = canonical_key(new_path);
                table.by_path[new_key] = self;
                table.failed.erase(new_key);
            }

            // 重载除清空绑定/失败记录外，还须解除旧文档与路径的关联：
            // resolve 的反查会按文档自身路径复活旧实例，若不解除，重载
            // 语义（下次打开强制从磁盘重读）会被破坏。
            template <typename T>
            void detach(doc_table<T>& table, const std::string& key)
            {
                auto fnd = table.by_path.find(key);
                if (fnd != table.by_path.end())
                {
                    if (auto doc = live(fnd->second);
                        doc.has_value() && canonical_key((*doc)->path) == key)
                        (*doc)->path.clear();
                }
                table.by_path.erase(key);
                table.failed.erase(key);
            }

            // 注册表互斥锁。mutable：map_count/map_at/map_stamp 等只读
            // 接口亦须持锁（表被并发解析/释放改动）。
            mutable std::mutex m_mutex;

            doc_table<MapDocument> m_maps;
            doc_table<TilesetDocument> m_tilesets;
        };

        // ==================== 自动图块 ====================
        // 邻接掩码位（与旧版编辑器 WrappingTileWay 一致）
        constexpr int NB_U = 1, NB_D = 2, NB_L = 4, NB_R = 8;
        constexpr int NB_UL = 16, NB_LD = 32, NB_RU = 64, NB_RD = 128;

        // 计算某格瓦片值的 8 邻接掩码：邻居与该格存储值完全相同才算连接
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
            // 2. 瓦片默认 -> 3. schema 默认值（瓦片没有元数据即默认瓦片）
            int32_t v = doc->layers[layer].grid[y * doc->width + x];
            if (v != 0)
            {
                auto cell = decode_tile_value(v);
                if (cell.has_value()
                    && cell->tileset_idx >= 0
                    && cell->tileset_idx < (int32_t)doc->tilesets.size()
                    && doc->tilesets[cell->tileset_idx].resolved.has_value())
                {
                    const auto* ts = doc->tilesets[cell->tileset_idx].resolved->get();
                    const auto* def = ts->tile_def(cell->local_id);
                    if (def != nullptr)
                    {
                        auto fnd = def->properties.find(name);
                        if (fnd != def->properties.end())
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
            auto cell = decode_tile_value(v);
            if (!cell.has_value()
                || cell->tileset_idx < 0
                || cell->tileset_idx >= (int32_t)doc->tilesets.size()
                || !doc->tilesets[cell->tileset_idx].resolved.has_value())
                return true;
            const auto* ts = doc->tilesets[cell->tileset_idx].resolved->get();
            return ts->tile_walkable(cell->local_id);
        }

        // ==================== 单元格渲染解析（唯一权威实现） ====================
        // 把某格解析为 1~4 个带 UV 的子四边形。网格构建（TilemapSystem）
        // 与编辑器预览（je_tilemap_cell_quad*）共用本实现，保证两者所见
        // 永远一致（含自动图块的邻接象限解析）。
        struct cell_quad_info
        {
            int32_t tileset_idx = 0;        // 地图引用表中的图集下标
            int32_t source_idx = 0;         // 图集内源纹理下标
            float u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f; // tile-local UV（v0 下 v1 上，[0,1]²）
            int32_t layer = 0;              // 数组纹理层号（每层一个瓦片）
            // 目标象限：nullopt = 整格；0=LU 1=RU 2=LD 3=RD（四象限自动图块）
            std::optional<int32_t> corner;
        };

        // 通用网格数组构建：把 2D 纹理按 (xcount × ycount) 网格逐格拷贝
        // 为"每层一个瓦片"的数组纹理，层号 = iy*xcount + ix，层尺寸为
        // cell_w × cell_h。图集展开为数组后，分片采样天然不会越出瓦片
        // 自身（层内 CLAMP），相邻瓦片渗色在结构上不可能发生。图像与
        // 记录网格不符（图片被替换/裁剪）时按可用范围收敛构建，超出部分
        // 的层保持黑色并告警。纹理未就绪/不可读返回 nullopt，可稍后重试。
        inline std::optional<basic::resource<graphic::texture_array>>
            build_grid_array(const std::string& path,
                int32_t xcount, int32_t ycount, int32_t cell_w, int32_t cell_h)
        {
            auto tex = graphic::texture2d::load(nullptr, path);
            if (!tex.has_value())
                return std::nullopt;
            auto* raw = tex.value()->resource();
            if (raw->m_pixels == nullptr
                || cell_w <= 0 || cell_h <= 0 || xcount <= 0 || ycount <= 0)
                return std::nullopt;

            const size_t cw = (size_t)cell_w, ch = (size_t)cell_h;
            const int32_t tw = (int32_t)raw->m_width;
            const int32_t th = (int32_t)raw->m_height;

            const int32_t xc = std::min(xcount, tw / cell_w);
            const int32_t yc = std::min(ycount, th / cell_h);
            if (xc <= 0 || yc <= 0)
                return std::nullopt;
            if (xc != xcount || yc != ycount)
                debug::logwarn(
                    "Tilemap: texture '%s' smaller than recorded grid, "
                    "array layers beyond image will be blank.",
                    path.c_str());

            // Keep the atlas' color-space flag: the layer pixels are copied
            // raw, so the array must decode sRGB exactly like the source.
            auto arr = graphic::texture_array::create(
                cw, ch, (size_t)(xcount * ycount),
                (jegl_texture_common::format)(
                    jegl_texture_common::format::RGBA
                    | (raw->m_format & jegl_texture_common::format::SRGB)));
            uint8_t* dst = arr->pixels();

            for (int32_t ty = 0; ty < yc; ++ty)
            {
                for (int32_t tx = 0; tx < xc; ++tx)
                {
                    uint8_t* dst_layer =
                        dst + (size_t)((ty * xcount + tx) * cell_w * cell_h) * 4;
                    // 2D 纹理经 stbi_set_flip_vertically_on_load 加载，
                    // m_pixels 为自底向上行序（缓冲行 0 = 图像底行），
                    // 瓦片 (tx,ty) 在缓冲中位于行 [th-(ty+1)*ch, th-ty*ch)。
                    // 层行序沿用该翻转序（层行 0 = 瓦片底行，采样 v=1 =
                    // 瓦片顶），与 tile-local UV 及编辑器换算的 v=1 为顶
                    // 约定一致。
                    const size_t buf_row0 = (size_t)(th - (ty + 1) * cell_h);
                    for (size_t r = 0; r < ch; ++r)
                        memcpy(
                            dst_layer + r * cw * 4,
                            raw->m_pixels
                                + ((buf_row0 + r) * (size_t)tw
                                    + (size_t)(tx * cell_w)) * 4,
                            cw * 4);
                }
            }
            return arr;
        }

        // 构建主纹理（反照率）的数组纹理：层尺寸 = tile_px
        inline bool build_source_array(SourceTexture& src)
        {
            if (src.tile_px <= 0)
                return false;
            auto arr = build_grid_array(src.path, src.xcount, src.ycount,
                src.tile_px, src.tile_px);
            if (!arr.has_value())
                return false;
            src.array = arr.value();
            return true;
        }

        // 构建附加层级（如法线图集）的数组纹理：与主纹理同一网格与层号
        //（iy*xcount + ix），cell 尺寸由该层级自身像素尺寸与主网格推得
        //（tex_w/xcount × tex_h/ycount），支持与主纹理不同分辨率的对齐；
        // 未测量尺寸时按主纹理 cell 尝试。
        inline bool build_extra_array(SourceTexture& src, SourceExtra& extra)
        {
            int32_t cw = src.tile_px, ch = src.tile_px;
            if (extra.tex_w.has_value() && extra.tex_h.has_value()
                && src.xcount > 0 && src.ycount > 0)
            {
                cw = *extra.tex_w / src.xcount;
                ch = *extra.tex_h / src.ycount;
                if (cw <= 0 || ch <= 0)
                    return false;
            }
            auto arr = build_grid_array(extra.path, src.xcount, src.ycount, cw, ch);
            if (!arr.has_value())
                return false;
            extra.array = arr.value();
            return true;
        }

        // UV 矩形（数组采样：tile-local [0,1]²，v0 下 v1 上）与数组层号
        //（layer = iy * xcount + ix，每层一个瓦片）
        inline void tile_uv(const SourceTexture& src, int32_t ix, int32_t iy,
            float& u0, float& v0_bot, float& u1, float& v1_top, int32_t& layer)
        {
            u0 = 0.f;
            u1 = 1.f;
            v1_top = 1.f;
            v0_bot = 0.f;
            layer = iy * src.xcount + ix;
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
            auto cell = decode_tile_value(v);
            if (!cell.has_value()
                || cell->tileset_idx < 0
                || cell->tileset_idx >= (int32_t)doc.tilesets.size()
                || !doc.tilesets[cell->tileset_idx].resolved.has_value())
                return 0;
            const int32_t ts_idx = cell->tileset_idx;
            const int32_t tile_id = cell->local_id;
            const auto* ts = doc.tilesets[ts_idx].resolved->get();

            // 瓦片 id -> 图集格（不属于任何现存源即不可解析）
            const auto hit = ts->tile_cell(tile_id);
            if (!hit.has_value())
                return 0;
            const int32_t source_idx = std::get<0>(*hit);
            const int32_t ix = std::get<1>(*hit);
            const int32_t iy = std::get<2>(*hit);
            const auto& src = ts->sources[source_idx];
            const je_TileKind kind = ts->tile_kind(tile_id);
            const TileDef* def = ts->tile_def(tile_id);

            // 整格取件：按原样绘制 / 普通瓦片 / blob47 变体
            auto fill_full = [&](int32_t src_idx, int32_t cx, int32_t cy)
            {
                out[0].tileset_idx = ts_idx;
                out[0].source_idx = src_idx;
                tile_uv(ts->sources[src_idx], cx, cy,
                    out[0].u0, out[0].v0, out[0].u1, out[0].v1, out[0].layer);
                out[0].corner = std::nullopt;
            };

            // 正值 = 按原样绘制；普通瓦片的自动模式与之一致
            if (!cell->is_auto || kind == JE_TILE_KIND_PLAIN)
            {
                fill_full(source_idx, ix, iy);
                return 1;
            }

            if (kind == JE_TILE_KIND_RPGMAKER_XP
                || kind == JE_TILE_KIND_RPGMAKER_VX)
            {
                // RPGMaker 四象限（XP 式条带 / VX 式 2x3 块）：
                // 槽位序 [LU, RU, LD, RD]
                int mask = compute_mask(doc, layer, x, y, v);
                if (kind == JE_TILE_KIND_RPGMAKER_VX && mask == 0)
                {
                    // VX 孤块：mkxp 变体47 = 锚点整块
                    fill_full(source_idx, ix, iy);
                    return 1;
                }
                struct corner_bits { int o1, o2, diag; };
                const corner_bits cb[4] = {
                    { NB_U, NB_L, NB_UL }, { NB_U, NB_R, NB_RU },
                    { NB_D, NB_L, NB_LD }, { NB_D, NB_R, NB_RD },
                };
                for (int c = 0; c < 4; ++c)
                {
                    half_cell hc = rpgmaker_quadrant(c,
                        corner_state(mask, cb[c].o1, cb[c].o2, cb[c].diag));
                    // 半格 (col,row) 跨锚点起的 2x3 图块区域（col∈[0,4)、
                    // row∈[0,6)，即 4x6 个半格）：所在瓦片 = 锚点 +
                    // (col/2, row/2)，层内半格 = (col%2, row%2)，行自瓦片
                    // 顶部向下增长。块越出图集网格时钳到边界层（原像素
                    // UV 实现同样越界采样，行为对齐且层号必须合法）。
                    const int32_t layer = std::clamp(
                        (iy + hc.row / 2) * src.xcount + (ix + hc.col / 2),
                        0, src.xcount * src.ycount - 1);
                    out[c].tileset_idx = ts_idx;
                    out[c].source_idx = source_idx;
                    out[c].u0 = (hc.col % 2) * 0.5f;
                    out[c].u1 = (hc.col % 2) * 0.5f + 0.5f;
                    out[c].v1 = 1.f - (hc.row % 2) * 0.5f;
                    out[c].v0 = 1.f - (hc.row % 2) * 0.5f - 0.5f;
                    out[c].layer = layer;
                    out[c].corner = c;
                }
                return 4;
            }

            // blob47：按邻接掩码解析为一个整块变体（显式变体表，或自
            // 锚点起行优先连续取 47 块的默认布局）
            int mask = compute_mask(doc, layer, x, y, v);
            uint8_t variant = blob47_table()[(uint8_t)(mask & 0xFF)];
            if (def != nullptr && (size_t)variant < def->variant_tiles.size())
            {
                const auto variant_hit = ts->tile_cell(def->variant_tiles[variant]);
                if (!variant_hit.has_value())
                    return 0;
                fill_full(std::get<0>(*variant_hit),
                    std::get<1>(*variant_hit), std::get<2>(*variant_hit));
                return 1;
            }
            // 默认布局：锚点格起行优先 + variant（块越出网格即不可解析）
            const int32_t flat = iy * src.xcount + ix + variant;
            if (flat < 0 || flat >= src.xcount * src.ycount)
                return 0;
            fill_full(source_idx, flat % src.xcount, flat / src.xcount);
            return 1;
        }
    }
}
