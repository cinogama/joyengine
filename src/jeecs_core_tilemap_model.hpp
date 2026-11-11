#pragma once

// Tilemap 文档模型层：数据结构、.je4tileset/.je4tilemap 的 TOML 源缓存
// 读取（运行期只读 <源>.je4cache，由 Woolang 生成器从 TOML 生成）、
// 进程内文档注册表、自动图块（RPGMaker 四象限 / blob47）邻接解析、
// 属性取值链，以及“某格渲染成哪些子四边形”的统一解析器
//（resolve_cell_quads —— 网格构建与编辑器预览共用同一实现）。
//
// 本头文件只保留数据结构、缓存格式说明与函数声明（瓦片值编码等
// 平凡小函数仍为 inline），实现统一在 jeecs_core_tilemap_model.cpp；
// 渲染系统 TilemapSystem 与 je_tilemap_* C API 见
// jeecs_core_tilemap_system.hpp。

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#ifndef JE_ENABLE_DEBUG_API
#   define JE_ENABLE_DEBUG_API
#endif
#include "jeecs/basic.hpp"
#include "jeecs/cache.hpp"
#include "jeecs/file.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/math.hpp"
#include "jeecs/tilemap.hpp"

#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <tuple>
#include <string>
#include <memory>
#include <cstring>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <mutex>
#include <variant>
#include <optional>
#include <array>
#include <iterator>

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
        // je_tile_kind_t 非 PLAIN 的瓦片；负值只表达"这次按自动拼接绘制"
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
        // 属性值的类型化存储：variant 备选序与 je_property_type_t 枚举值
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
        property_value property_zero_value(je_property_type_t t);

        // ==================== 文档数据模型 ====================
        // 图集源纹理的附加层级：pass 为绑定通道 id（>= 1；主纹理固定
        // 占通道 0，与图层样式"槽 0 保留给图集源纹理"约定一致）。
        // 必须定义在 SourceTexture 之前——其后者的 vector<SourceExtra>
        // 成员要求此类型完整（libc++ 在头文件内实例化 vector 析构）
        struct SourceExtra
        {
            int32_t pass;
            std::string path;   // 运行时路径（@/ 或 !/）
            // 纹理像素尺寸（添加时测量，cell 尺寸由此与主纹理网格推得）
            std::optional<int32_t> tex_w, tex_h;
            // 运行期：与主纹理同层号对齐的数组纹理（build_extra_array
            // 构建；不参与序列化）。nullopt = 尚未构建
            std::optional<basic::resource<graphic::texture_array>> array;
            // 运行期：构建失败标记（同 SourceTexture::array_failed）
            bool array_failed;

            SourceExtra()
                : pass(1)
                , array_failed(false)
            {
            }
        };
        struct SourceTexture
        {
            std::string path;   // 运行时路径（@/ 或 !/）
            int32_t tile_px;
            int32_t xcount, ycount;
            // 每格的瓦片 id（行优先，长度恒 = xcount*ycount）。添加源时
            // 按行优先连续分配；纹理尺寸变化后已有格保留原 id、新增格
            // 从文档的 next_tile_id 追加——地图按 id 引用瓦片不会漂移，
            // 因此 id 与格坐标不是固定公式关系。0 = 尚未分配（损坏）。
            std::vector<int32_t> cell_ids;
            // 纹理像素尺寸（UV 计算用）；nullopt = 未测量，按网格推算。
            // 有值时恒为正
            std::optional<int32_t> tex_w, tex_h;

            // 运行期：每层一个瓦片的数组纹理（build_source_array 构建，
            // 分片网格经 tex2darray 采样；不参与序列化）。
            // nullopt = 尚未构建（分片扫描会兜底重试）
            std::optional<basic::resource<graphic::texture_array>> array;
            // 运行期：构建失败标记（纹理不可读时只试一次，避免逐帧兜底
            // 重试刷日志；图集重载后随新实例重置）
            bool array_failed;

            // 附加层级纹理（如法线图集）：与主纹理共用同一瓦片网格
            //（xcount/ycount/tile 索引），按各自 pass 展开为独立数组纹理
            std::vector<SourceExtra> extras;

            SourceTexture()
                : tile_px(32)
                , xcount(1)
                , ycount(1)
                , array_failed(false)
            {
            }
        };
        // 瓦片默认法线：指向屏幕外侧（引擎内 -Z 为朝向摄像机的方向）。
        // 参与网格顶点生成，瓦片的显式法线与之相同即视为默认（可从稀疏
        // 元数据表移除）。
        inline constexpr std::array<float, 3> DEFAULT_TILE_NORMAL{ 0.f, 0.f, -1.f };

        // 瓦片元数据：图集格 (source, ix, iy) 上的稀疏定义。没有条目的
        // 格子按默认值处理（可通行、普通瓦片、无属性、名称为空）。所有
        // 字段都只描述"这块瓦片本身"，自动图块（地形）也只是 kind 非
        // PLAIN 的瓦片，不再有独立的属性/通行性表。
        struct TileDef
        {
            std::string name;
            bool walkable;
            je_tile_kind_t kind;  // 自动拼接类型
            // 顶点法线（局部空间单位向量，默认 DEFAULT_TILE_NORMAL）：
            // 网格生成时写入该瓦片全部顶点，供光照判定"这块瓦片朝哪"。
            // 自动拼接格整格共享锚点瓦片的法线（四象限/变体不各自取用）。
            std::array<float, 3> normal;
            std::vector<int32_t> variant_tiles;     // blob47 显式变体表（瓦片 id）
            std::map<std::string, property_value> properties;   // 瓦片默认属性

            TileDef()
                : walkable(true)
                , kind(JE_TILE_KIND_PLAIN)
                , normal(DEFAULT_TILE_NORMAL)
            {
            }
        };
        struct PropertyDef
        {
            std::string name;
            je_property_type_t type;
            property_value default_value;    // 与 type 一致

            PropertyDef()
                : type(JE_PROPERTY_TYPE_BOOL)
            {
            }
        };

        struct TilesetDocument
        {
            std::string name;
            std::string path;   // .je4tileset 运行时路径；未保存为空
            std::vector<SourceTexture> sources;
            // 稀疏瓦片元数据：瓦片 id -> 定义（无条目 = 默认瓦片）
            std::map<int32_t, TileDef> tiles;
            // 瓦片 id -> (源下标, ix, iy) 反向索引（只读查询用；sources
            // 或各源的格 id 表变化后经 rebuild_tile_cells 重建，不落盘）
            std::unordered_map<int32_t, std::array<int32_t, 3>> tile_cells;
            // 下一个可分配的瓦片 id（只增不减：删除源/裁掉格子后其 id
            // 作废，已有地图按 id 引用不会漂移到别的格子）
            int32_t next_tile_id;
            std::vector<PropertyDef> properties;

            uint64_t version;

            TilesetDocument()
                : next_tile_id(1)
                , version(1)
            {
            }

            // 重建 id -> 格 反向索引（先整表构建再换入，读者不会看到
            // 半成品索引）
            void rebuild_tile_cells();
            // 某图集格子的瓦片 id（参数越界或源未分配 id 时返回 0）
            int32_t tile_id_at(int32_t source_idx, int32_t ix, int32_t iy) const;
            // 瓦片 id -> (源下标, ix, iy)；不属于任何现存格子返回 nullopt
            std::optional<std::tuple<int32_t, int32_t, int32_t>>
                tile_cell(int32_t tile_id) const;
            const TileDef* tile_def(int32_t tile_id) const;
            // 取（必要时创建）瓦片元数据；调用方须保证 tile_id 属于
            // 某个现存格子
            TileDef& ensure_tile_def(int32_t tile_id);
            bool tile_walkable(int32_t tile_id) const;
            je_tile_kind_t tile_kind(int32_t tile_id) const;
            // 元数据是否已回到默认（默认条目可从稀疏表移除）
            static bool tile_def_is_default(const TileDef& def);
            // 元数据编辑器收尾：默认条目从稀疏表移除
            void compact_tile_def(int32_t tile_id);
        };

        // ==================== 图层着色器样式 ====================
        // 图层样式直接持有渲染资源实例（basic::resource 为共享指针，复制
        // 仍指向同一实例）：uniform 值储存在着色器实例本身，由实例的
        // 持有方直接维护。落盘时着色器/纹理按资源句柄的来源路径记录，
        // uniform 值读自实例（JE_ 引擎内置与 TEXTURE/矩阵类除外，与
        // .je4entity 保存 Shaders 的口径一致），加载后写回新实例。
        struct LayerTexture
        {
            int32_t slot;       // 槽 0 保留给图集源纹理
            basic::resource<graphic::texture2d> texture;

            LayerTexture(int32_t slot_, basic::resource<graphic::texture2d> texture_)
                : slot(slot_)
                , texture(std::move(texture_))
            {
            }
        };
        struct LayerStyle
        {
            // 空 = 未指定（分片着色器列表为空）
            std::optional<basic::resource<graphic::shader>> shader;
            std::vector<LayerTexture> textures;         // 附加纹理（槽 >= 1）
            // 样式独立版本戳：样式变化不 bump doc->version，避免触发
            // 全网格重扫与顶点重建；渲染侧只做轻量的着色器/纹理对齐。
            // 不落盘（加载即 1）。
            uint64_t style_version;

            LayerStyle()
                : style_version(1)
            {
            }
        };

        // 新建图层的缺省渲染队列基数：rend_queue = 基数 + 层下标。与场景
        // 实体常用的 0..N 小队列值错开一个数量级，保证地图默认不被同值
        // 队列的普通实体插入绘制序列
        constexpr int32_t LAYER_REND_QUEUE_BASE = 10000;

        struct MapLayer
        {
            std::string name;
            bool visible;
            bool locked;    // 编辑器锁定标记（运行时忽略）
            // 该层在世界中的 z 偏移（相对地图根实体，世界单位）。
            // 不同层须错开 z 以避免深度冲突；缺省按层下标错开。
            float z;
            // 该层分片的渲染队列值（Renderer::Rendqueue）：同图各层按
            // 此值升序绘制（画家算法，层间叠放的权威顺序）；z 只在与
            // 写深度的场景物体之间参与裁决。缺省按基数 + 层下标错开。
            int32_t rend_queue;
            // 该层的 2D 光照自阴影行为（je_layer_self_shadow_t）：OFF = 该层
            // 分片不挂 Light2D::SelfShadow 组件；ON/AUTO = 挂载且
            // auto_disable 分别固定为 false/true。分片组件由
            // TilemapSystem 逐帧对齐（增删走命令缓冲）。
            je_layer_self_shadow_t selfshadow;
            // 自阴影覆盖强度（写入分片 SelfShadow::factor，即覆盖色
            // RGBA 乘子，0..1 常用）：仅 selfshadow != OFF 时生效
            float selfshadow_factor;
            // 该层的 2D 光照遮挡阴影行为（je_layer_block_shadow_t）：OFF =
            // 该层分片不挂 Light2D::BlockShadow 组件；ON/AUTO = 挂载且
            // auto_disable 分别固定为 false/true，factor 取 blockshadow_factor。
            // 遮挡轮廓（实心格边界，贪心合并）由 TilemapSystem 按分块
            // 网格生成到组件的 m_block_mesh（随版本戳重建）。
            je_layer_block_shadow_t blockshadow;
            // 遮挡阴影强度（写入分片 BlockShadow::factor，阴影体浓度，
            // 0..1 常用）：仅 blockshadow != OFF 时生效
            float blockshadow_factor;
            std::vector<int32_t> grid;  // w*h 稠密网格，行优先、y 自上而下
            LayerStyle style;           // 逐层着色器样式（可空）

            MapLayer()
                : visible(true)
                , locked(false)
                , z(0.f)
                , rend_queue(LAYER_REND_QUEUE_BASE)
                , selfshadow(JE_LAYER_SELFSHADOW_OFF)
                , selfshadow_factor(1.f)
                , blockshadow(JE_LAYER_BLOCKSHADOW_OFF)
                , blockshadow_factor(1.f)
            {
            }
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
            int32_t width, height, tile_px;
            std::vector<TilesetRef> tilesets;   // 引用的图集（路径 + 解析结果）
            std::vector<MapLayer> layers;
            // 逐格属性覆盖：(layer, x + y*width) -> (name -> value)
            std::map<std::pair<int32_t, int32_t>, std::map<std::string, property_value>> cell_properties;
            // je_tilemap_cell_find 的结果缓存（find_get 逐个取回），随
            // 文档实例生死，重载/销毁后自然从空表开始
            std::vector<std::pair<int32_t, int32_t>> find_result;

            uint64_t version;

            MapDocument()
                : width(0)
                , height(0)
                , tile_px(32)
                , version(1)
            {
            }

            bool in_bounds(int32_t layer, int32_t x, int32_t y) const
            {
                return layer >= 0 && layer < (int32_t)layers.size()
                    && x >= 0 && x < width && y >= 0 && y < height;
            }
        };

        // ==================== 缓存文档读写 ====================
        // .je4tileset/.je4tilemap 是 TOML 文本源，运行期读取由 Woolang
        // 生成器写出的 <源路径>.je4cache。缓存文件头（格式版本/源 CRC64）
        // 由 je_cache_file_create 写入，管理器版本留空占位，由
        // je_cache_file_close(..., true) 回写后缓存才生效，其后是一个
        // jeecs_cache_binary_* 文档：
        //   图集 DOCUMENT
        //     "name" -> STRING
        //     "next_tile_id" -> INT32
        //     "sources" -> LIST of DOCUMENT
        //         "path" -> STRING, "tile_px"/"xcount"/"ycount" -> INT32
        //         "tex_w"/"tex_h" -> INT32（0 = 未测量）
        //         "cell_ids" -> BUFFER（int32，行优先，长度 = xcount*ycount）
        //         "extras" -> LIST of DOCUMENT
        //             "pass" -> INT32, "path" -> STRING
        //             "tex_w"/"tex_h" -> INT32（0 = 未测量）
        //     "properties" -> LIST of DOCUMENT
        //         "name" -> STRING, "type" -> INT32, "default" -> 类型化值
        //     "tiles" -> LIST of DOCUMENT
        //         "id" -> INT32, "name" -> STRING, "walkable" -> BOOL
        //         "kind" -> INT32, "normal" -> LIST of 3 FLOAT32
        //         "variants" -> BUFFER（int32）
        //         "properties" -> LIST of DOCUMENT
        //             "name" -> STRING, "value" -> 类型化值（含 "type"）
        //   地图 DOCUMENT
        //     "width"/"height"/"tile_px" -> INT32
        //     "tilesets" -> LIST of STRING
        //     "layers" -> LIST of DOCUMENT
        //         "name" -> STRING, "visible"/"locked" -> BOOL
        //         "z" -> FLOAT32, "rend_queue" -> INT32
        //         "selfshadow"/"blockshadow" -> INT32
        //         "selfshadow_factor"/"blockshadow_factor" -> FLOAT32
        //         "grid" -> BUFFER（int32，行优先，长度 = width*height）
        //         "shader" -> STRING（空 = 未指定）
        //         "textures" -> LIST of DOCUMENT
        //             "slot" -> INT32, "path" -> STRING
        //         "uniforms" -> LIST of DOCUMENT
        //             "name" -> STRING, "type" -> INT32（0..7）, "value" -> 值
        //     "cell_properties" -> LIST of DOCUMENT
        //         "layer"/"x"/"y" -> INT32
        //         "properties" -> LIST of DOCUMENT
        //             "name" -> STRING, "type" -> INT32, "value" -> 值
        // 类型化属性值的 "type" 即 je_property_type_t；uniform 类型标记：
        //   0 INT / 1 INT2 / 2 INT3 / 3 INT4 /
        //   4 FLOAT / 5 FLOAT2 / 6 FLOAT3 / 7 FLOAT4
        // 值形态：BOOL/INT32/FLOAT32 为标量，STRING 为字符串；VEC2/3/4 与
        // 多分量 uniform 为对应分量数的 LIST。

        // 缓存文件所有者：任何失败路径都保证关闭
        struct cache_file_holder
        {
            je_file_t* m_file;

            explicit cache_file_holder(je_file_t* file)
                : m_file(file)
            {
                assert(file != nullptr);
            }
            ~cache_file_holder()
            {
                if (m_file != nullptr)
                    ::je_file_close(m_file);
            }
            cache_file_holder(const cache_file_holder&) = delete;
            cache_file_holder& operator=(const cache_file_holder&) = delete;
        };

        bool read_cache_string_value(
            const je_cache_binary_string_t* str, std::string* out);

        bool read_cache_string(const je_cache_binary_document_t* document,
            const char* key, std::string* out);

        bool read_cache_int32(const je_cache_binary_document_t* document,
            const char* key, int32_t* out);

        bool read_cache_float32(const je_cache_binary_document_t* document,
            const char* key, float* out);

        bool read_cache_bool(const je_cache_binary_document_t* document,
            const char* key, bool* out);

        // 读取 LIST 中的 count 个标量（VEC2/3/4、法线、多分量 uniform）
        bool read_cache_float_list(
            const je_cache_binary_list_t* list, float* out, size_t count);

        bool read_cache_int32_list(
            const je_cache_binary_list_t* list, int32_t* out, size_t count);

        // 读取缓冲区到 int32 数组；expected_count < 0 表示长度任意
        bool read_cache_int32_buffer(
            const je_cache_binary_document_t* document, const char* key,
            int64_t expected_count, std::vector<int32_t>* out);

        // 读取一个类型化属性值（文档内 "type" + value_key 指向的值）。
        // 图集 schema 的属性默认值用 "default"，瓦片/格属性用 "value"。
        bool read_cache_property_value(
            const je_cache_binary_document_t* document, const char* value_key,
            property_value* out, /* OPTIONAL */ je_property_type_t* out_type);

        // @/!/ 前缀 -> 本地文件系统路径（注册表路径键规范化用；读取走
        // je_file_open 支持 fimg 包）
        std::string resolve_runtime_prefix(const std::string& path);

        // 从缓存加载文档（失败返回 nullopt；由 documents 注册表经
        // resolve 调用，也可直接使用）
        std::optional<std::shared_ptr<TilesetDocument>>
            load_tileset_from(const std::string& path);

        std::optional<std::shared_ptr<MapDocument>>
            load_map_from(const std::string& path);

        // ==================== 源网格跟随纹理尺寸 ====================
        // 以纹理当前像素尺寸重新推导源的瓦片网格（格数 = 尺寸 / tile_px）。
        // 纹理增加一行/一列时，源随之新增一行/一列瓦片：已有格保留原瓦片
        // id（地图按 id 引用不会漂移），新增格从文档的 next_tile_id 追加
        // 新 id；纹理缩小被裁掉的格连同其元数据一并丢弃。纹理不可读或
        // 不足一块瓦片时保持记录的网格不变。返回是否改动了文档。
        bool reconcile_source_grid(TilesetDocument& doc, int32_t source_idx);

        // 把缓存文档里的 uniform 值写回着色器实例（名字/类型与声明不符时
        // 由 set_uniform 校验兜底：找不到名字为无操作，类型不符告警）。
        // type 为缓存文档中的 0..7 标记（INT..FLOAT4）。
        bool apply_cache_uniform(graphic::shader* shader,
            const je_cache_binary_document_t* document, int32_t type);

        // ==================== 文档注册表 ====================
        // 进程内全局唯一：按路径共享文档实例，编辑器与世界经由同一份
        // 数据实现"未保存修改实时同步"。对外以文档实例指针（je_Tilemap
        // _Document*/je_tileset_document_t*）标识，指针身份即文档身份。
        //
        // ---- 所有权模型 ----
        // 文档实例的存活完全由 shared_ptr 决定：外部（C ABI）引用由
        // 注册表条目代为强持有（refs > 0 期间）；引用方（地图的
        // TilesetRef::resolved、TilemapSystem 经 observe 取得的句柄）
        // 各自直接持有。注册表自身只经 weak_ptr 观察实例，不为自己
        // 缓存目的保活任何文档。外部引用归零且无引用方时实例销毁
        //（含未保存文档——unload 即丢弃），下次按路径 open 从磁盘重
        // 读；地图销毁会级联释放其图集持有（RAII，无手工逐出）。
        // 路径 -> 实例是派生索引：绑定/重载经 bind_*/reload_* 集中
        // 维护，实例销毁后 weak 引用自然过期，解析时按需清除。失败
        // 路径只尝试一次，避免逐帧重试造成日志刷屏；绑定/重载会清除
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
            int32_t map_count() const;
            std::optional<std::shared_ptr<MapDocument>> map_at(int32_t index) const;

            // 快照枚举：返回当前全部存活地图文档，每个各带 1 个外部
            // 引用（调用方用毕须 unload，Woolang 侧即 gchandle 终结器
            // 路径）。在一次持锁内完成"收集 + 计引用"，避免逐个
            // count/at + keep 之间文档被并发释放的竞态。
            std::vector<MapDocument*> snapshot_maps();

            // 打开（或复用缓存）文档；失败返回 nullopt。成功返回的实例
            // 已带 1 个外部引用（在锁内完成"解析 + 计引用"），调用方
            // 用毕 unload——这消除了"先返回指针、后 keep"窗口内被并发
            // 终结器释放的竞态。
            std::optional<std::shared_ptr<MapDocument>> open_map(const std::string& path);
            std::optional<std::shared_ptr<TilesetDocument>> open_tileset(const std::string& path);

            // 观察：与 open 同一解析逻辑，但返回的持有不占外部引用计数
            //——供渲染系统等以 RAII 方式保活（析构自动让出），不参与
            // C ABI 的 keep/unload 记账。
            std::optional<std::shared_ptr<MapDocument>> observe_map(const std::string& path);
            std::optional<std::shared_ptr<TilesetDocument>> observe_tileset(const std::string& path);

            // 注册新建（未落盘）文档，返回已带 1 个外部引用的实例指针
            MapDocument* register_map(std::shared_ptr<MapDocument> doc);
            TilesetDocument* register_tileset(std::shared_ptr<TilesetDocument> doc);

            // 编辑器已把 TOML 源写入 path：把文档的路径登记迁移到 path。
            // 不写盘、不生成缓存——<path>.je4cache 由下次加载经
            // je_tilemap_cache_open_or_create 按源 CRC 自然重建。
            // 重绑路径索引（另一文档对同路径的既有绑定被顶替 ——
            // "最后一次保存者获胜"，与编辑器另存语义一致）。调用方须
            // 持有外部引用（无账目即契约外使用，按失败返回）。
            bool bind_map_path(MapDocument* doc, const std::string& path);
            bool bind_tileset_path(TilesetDocument* doc, const std::string& path);

            // 丢弃缓存中的未保存修改：解除路径绑定，下次访问从磁盘重读。
            // 仍被引用的既有实例不受影响（其路径被清空后不再能按路径
            // 复活），随最后一个引用方销毁。
            void reload_map(const std::string& path);
            void reload_tileset(const std::string& path);

            // ---- 外部引用计数（keep/unload）----
            // keep：外部引用 +1，返回新计数。调用方持有的指针必然有
            // 引用账目（refs >= 1 期间实例不会被销毁），无账目即调用方
            // 传入了伪造/已失效指针，按无操作兜底返回 0。
            int32_t keep_map(MapDocument* doc);
            int32_t keep_tileset(TilesetDocument* doc);

            // unload：外部引用 -1（下限 0）；归零即撤销注册表的强持有
            //——已无引用方时实例就地销毁（此后指针失效），下次按路径
            // open 从磁盘重读。返回剩余计数。GC 终结器可能在任意线程
            // 并发调用：减计数与销毁在同一持锁区间内完成，与并发 keep/
            // open 互斥；对已销毁实例（双重释放）按无操作兜底。
            int32_t unload_map(MapDocument* doc);
            int32_t unload_tileset(TilesetDocument* doc);

            // 保证地图文档的图集解析与注册表中的最新实例一致（处理图集
            // 被重载/替换/销毁的情况）：逐槽把 resolved 重新指向当前解
            // 析结果。持有即保活——同实例赋值为无操作，实例变化/解析
            // 失败时旧实例随赋值自动释放，无任何手工换绑记账。
            void ensure_tilesets(MapDocument& m);

            // 地图引用表增删（删除项的图集持有随之自动释放）
            void append_tileset_entry(MapDocument& m, const std::string& path);
            void erase_tileset_entry(MapDocument& m, size_t idx);

            // 地图 + 其图集的合并版本戳：任一变化（含图集实例被重载/
            // 销毁后重载替换）都会改变。指针身份作实例标识（新实例重用
            // 旧地址且版本恰好相同的概率可忽略）；version 区分同实例内
            // 的编辑。持锁以避免与并发 ensure_tilesets 重排 resolved
            //（optional<shared_ptr> 写）竞争。
            uint64_t map_stamp(const MapDocument& m) const;

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
                    int32_t refs;

                    entry()
                        : refs(0)
                    {
                    }
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

            static uint64_t mix(uint64_t h, uintptr_t identity, uint64_t version);

            // 注册表键规范化：@/ 与 !/ 前缀解析为真实目录，反斜杠统一为
            // 正斜杠，连续斜杠折叠，Windows 下再统一小写（文件系统不分
            // 大小写）。无前缀的相对路径（浏览器工作目录形态）按运行时
            // 目录补全，与 je_file_open 的解析一致。同一文件的不同
            // 形态字符串必须映射到同一文档实例，否则编辑器两侧会各自
            // 持有独立副本，后一次保存会覆盖掉前一份里的新增数据。
            std::string canonical_key(const std::string& path) const;

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
        int compute_mask(const MapDocument& m, int32_t layer, int32_t x, int32_t y, int32_t encoded);

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
        half_cell rpgmaker_quadrant(int corner /*0=LU 1=RU 2=LD 3=RD*/, int state);

        int corner_state(int mask, int o1, int o2, int diag);

        // blob47 归约：256 个 8bit 掩码 -> 47 个变体号。
        // 两个掩码映射到同一变体，当且仅当四个角的状态各自相同
        //（0凸 1边 2凹 3内）；仅当两个相邻正交位都存在时对应对角位
        // 才参与区分。枚举序 = 正交掩码 0..15 为主序、相关对角组合为
        // 次序的首次出现序（即本引擎的规范 47 变体序）。
        const std::vector<uint8_t>& blob47_table();

        // ==================== 属性与通行性 ====================
        const property_value* resolve_property(
            MapDocument* doc, int32_t layer, int32_t x, int32_t y, const std::string& name);

        bool walkable_on(MapDocument* doc, int32_t layer, int32_t x, int32_t y);

        // ==================== 单元格渲染解析（唯一权威实现） ====================
        // 把某格解析为 1~4 个带 UV/法线的子四边形。网格构建（TilemapSystem）
        // 与编辑器预览（je_tilemap_cell_quad*）共用本实现，保证两者所见
        // 永远一致（含自动图块的邻接象限解析）。法线取自地图格存储的锚点
        // 瓦片（自动拼接的变体/象限不各自取用）。
        struct cell_quad_info
        {
            int32_t tileset_idx;        // 地图引用表中的图集下标
            int32_t source_idx;         // 图集内源纹理下标
            float u0, v0, u1, v1; // tile-local UV（v0 下 v1 上，[0,1]²）
            int32_t layer;              // 数组纹理层号（每层一个瓦片）
            // 顶点法线（局部空间单位向量，取自格子的锚点瓦片；自动拼接
            // 的整格/四象限变体共享锚点瓦片的法线）
            std::array<float, 3> normal;
            // 目标象限：nullopt = 整格；0=LU 1=RU 2=LD 3=RD（四象限自动图块）
            std::optional<int32_t> corner;

            cell_quad_info()
                : tileset_idx(0)
                , source_idx(0)
                , u0(0.f)
                , v0(0.f)
                , u1(0.f)
                , v1(0.f)
                , layer(0)
                , normal(DEFAULT_TILE_NORMAL)
            {
            }
        };

        // 通用网格数组构建：把 2D 纹理按 (xcount × ycount) 网格逐格拷贝
        // 为"每层一个瓦片"的数组纹理，层号 = iy*xcount + ix，层尺寸为
        // cell_w × cell_h。图集展开为数组后，分片采样天然不会越出瓦片
        // 自身（层内 CLAMP），相邻瓦片渗色在结构上不可能发生。图像与
        // 记录网格不符（图片被替换/裁剪）时按可用范围收敛构建，超出部分
        // 的层保持黑色并告警。纹理未就绪/不可读返回 nullopt，可稍后重试。
        std::optional<basic::resource<graphic::texture_array>>
            build_grid_array(const std::string& path,
                int32_t xcount, int32_t ycount, int32_t cell_w, int32_t cell_h);

        // 构建主纹理（反照率）的数组纹理：层尺寸 = tile_px
        bool build_source_array(SourceTexture& src);

        // 构建附加层级（如法线图集）的数组纹理：与主纹理同一网格与层号
        //（iy*xcount + ix），cell 尺寸由该层级自身像素尺寸与主网格推得
        //（tex_w/xcount × tex_h/ycount），支持与主纹理不同分辨率的对齐；
        // 未测量尺寸时按主纹理 cell 尝试。
        bool build_extra_array(SourceTexture& src, SourceExtra& extra);

        // UV 矩形（数组采样：tile-local [0,1]²，v0 下 v1 上）与数组层号
        //（layer = iy * xcount + ix，每层一个瓦片）
        void tile_uv(const SourceTexture& src, int32_t ix, int32_t iy,
            float& u0, float& v0_bot, float& u1, float& v1_top, int32_t& layer);

        // 解析某格的子四边形到 out（至少 4 项）；返回数量
        //（0 = 空格或不可解析）。
        int32_t resolve_cell_quads(MapDocument& doc, int32_t layer,
            int32_t x, int32_t y, cell_quad_info* out);
    }
}
