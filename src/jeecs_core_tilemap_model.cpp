// Tilemap 文档模型层的实现：缓存文档读取、源网格跟随纹理尺寸、
// 文档注册表、自动图块（RPGMaker 四象限 / blob47）邻接解析、属性
// 取值链与单元格渲染解析。数据结构、缓存格式说明与声明见
// jeecs_core_tilemap_model.hpp（各函数的契约注释也在该头文件）。

#include "jeecs_core_tilemap_model.hpp"

#include <cassert>
#include <cstring>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <mutex>

namespace jeecs
{
    namespace Tilemap
    {
        // ==================== 属性值 ====================
        property_value property_zero_value(je_property_type_t t)
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

        // ==================== TilesetDocument 查询 ====================
        void TilesetDocument::rebuild_tile_cells()
        {
            std::unordered_map<int32_t, std::array<int32_t, 3>> built;
            for (size_t si = 0; si < sources.size(); ++si)
            {
                const auto& s = sources[si];
                const int64_t count = (int64_t)s.xcount * s.ycount;
                if (count <= 0
                    || (int64_t)s.cell_ids.size() != count)
                    continue;
                for (int32_t iy = 0; iy < s.ycount; ++iy)
                    for (int32_t ix = 0; ix < s.xcount; ++ix)
                    {
                        const int32_t id =
                            s.cell_ids[(size_t)iy * s.xcount + ix];
                        if (id > 0)
                            built[id] = { (int32_t)si, ix, iy };
                    }
            }
            tile_cells = std::move(built);
        }

        int32_t TilesetDocument::tile_id_at(int32_t source_idx, int32_t ix, int32_t iy) const
        {
            if (source_idx < 0 || source_idx >= (int32_t)sources.size())
                return 0;
            const auto& s = sources[source_idx];
            if (ix < 0 || iy < 0 || ix >= s.xcount || iy >= s.ycount)
                return 0;
            const size_t idx = (size_t)iy * s.xcount + ix;
            if (idx >= s.cell_ids.size())
                return 0;
            return s.cell_ids[idx];
        }

        std::optional<std::tuple<int32_t, int32_t, int32_t>>
            TilesetDocument::tile_cell(int32_t tile_id) const
        {
            if (tile_id <= 0)
                return std::nullopt;
            auto fnd = tile_cells.find(tile_id);
            if (fnd == tile_cells.end())
                return std::nullopt;
            return std::make_tuple(
                fnd->second[0], fnd->second[1], fnd->second[2]);
        }

        const TileDef* TilesetDocument::tile_def(int32_t tile_id) const
        {
            auto fnd = tiles.find(tile_id);
            return fnd == tiles.end() ? nullptr : &fnd->second;
        }

        TileDef& TilesetDocument::ensure_tile_def(int32_t tile_id)
        {
            return tiles[tile_id];
        }

        bool TilesetDocument::tile_walkable(int32_t tile_id) const
        {
            const auto* def = tile_def(tile_id);
            return def == nullptr || def->walkable;
        }

        je_tile_kind_t TilesetDocument::tile_kind(int32_t tile_id) const
        {
            const auto* def = tile_def(tile_id);
            return def == nullptr ? JE_TILE_KIND_PLAIN : def->kind;
        }

        bool TilesetDocument::tile_def_is_default(const TileDef& def)
        {
            return def.name.empty() && def.walkable
                && def.kind == JE_TILE_KIND_PLAIN
                && def.normal == DEFAULT_TILE_NORMAL
                && def.variant_tiles.empty() && def.properties.empty();
        }

        void TilesetDocument::compact_tile_def(int32_t tile_id)
        {
            auto fnd = tiles.find(tile_id);
            if (fnd != tiles.end() && tile_def_is_default(fnd->second))
                tiles.erase(fnd);
        }

        // ==================== 缓存读取辅助 ====================
        bool read_cache_string_value(
            const je_cache_binary_string_t* str, std::string* out)
        {
            size_t len = 0;
            if (!::je_cache_binary_string_read(nullptr, &len, str))
                return false;

            out->resize(len);
            if (len > 0)
            {
                size_t capacity = len;
                if (!::je_cache_binary_string_read(out->data(), &capacity, str))
                    return false;
            }
            return true;
        }

        bool read_cache_string(const je_cache_binary_document_t* document,
            const char* key, std::string* out)
        {
            je_cache_binary_string_t str;
            if (!::je_cache_binary_document_index(
                document, key, JE_CACHE_BINARY_VALUE_STRING, &str))
                return false;

            return read_cache_string_value(&str, out);
        }

        bool read_cache_int32(const je_cache_binary_document_t* document,
            const char* key, int32_t* out)
        {
            return ::je_cache_binary_document_index(
                document, key, JE_CACHE_BINARY_VALUE_INT32, out);
        }

        bool read_cache_float32(const je_cache_binary_document_t* document,
            const char* key, float* out)
        {
            return ::je_cache_binary_document_index(
                document, key, JE_CACHE_BINARY_VALUE_FLOAT32, out);
        }

        bool read_cache_bool(const je_cache_binary_document_t* document,
            const char* key, bool* out)
        {
            return ::je_cache_binary_document_index(
                document, key, JE_CACHE_BINARY_VALUE_BOOL, out);
        }

        bool read_cache_float_list(
            const je_cache_binary_list_t* list, float* out, size_t count)
        {
            for (size_t i = 0; i < count; ++i)
                if (!::je_cache_binary_list_index(
                    list, i, JE_CACHE_BINARY_VALUE_FLOAT32, out + i))
                    return false;
            return true;
        }

        bool read_cache_int32_list(
            const je_cache_binary_list_t* list, int32_t* out, size_t count)
        {
            for (size_t i = 0; i < count; ++i)
                if (!::je_cache_binary_list_index(
                    list, i, JE_CACHE_BINARY_VALUE_INT32, out + i))
                    return false;
            return true;
        }

        bool read_cache_int32_buffer(
            const je_cache_binary_document_t* document, const char* key,
            int64_t expected_count, std::vector<int32_t>* out)
        {
            je_cache_binary_buffer_t buffer;
            if (!::je_cache_binary_document_index(
                document, key, JE_CACHE_BINARY_VALUE_BUFFER, &buffer))
                return false;

            size_t len = 0;
            if (!::je_cache_binary_buffer_read(nullptr, &len, &buffer))
                return false;
            if (len % sizeof(int32_t) != 0)
                return false;
            if (expected_count >= 0
                && (int64_t)(len / sizeof(int32_t)) != expected_count)
                return false;

            out->resize(len / sizeof(int32_t));
            if (len > 0)
            {
                size_t capacity = len;
                if (!::je_cache_binary_buffer_read(
                    out->data(), &capacity, &buffer))
                    return false;
            }
            return true;
        }

        bool read_cache_property_value(
            const je_cache_binary_document_t* document, const char* value_key,
            property_value* out, /* OPTIONAL */ je_property_type_t* out_type)
        {
            int32_t type = 0;
            if (!read_cache_int32(document, "type", &type)
                || !property_type_valid(type))
                return false;

            switch ((je_property_type_t)type)
            {
            case JE_PROPERTY_TYPE_BOOL:
            {
                bool value = false;
                if (!read_cache_bool(document, value_key, &value))
                    return false;
                *out = property_value{ value };
                break;
            }
            case JE_PROPERTY_TYPE_STRING:
            {
                std::string value;
                if (!read_cache_string(document, value_key, &value))
                    return false;
                *out = property_value{ std::move(value) };
                break;
            }
            case JE_PROPERTY_TYPE_INT:
            {
                int32_t value = 0;
                if (!read_cache_int32(document, value_key, &value))
                    return false;
                *out = property_value{ value };
                break;
            }
            case JE_PROPERTY_TYPE_FLOAT:
            {
                float value = 0.f;
                if (!read_cache_float32(document, value_key, &value))
                    return false;
                *out = property_value{ value };
                break;
            }
            case JE_PROPERTY_TYPE_VEC2:
            case JE_PROPERTY_TYPE_VEC3:
            case JE_PROPERTY_TYPE_VEC4:
            {
                const size_t count =
                    (size_t)(type - (int32_t)JE_PROPERTY_TYPE_VEC2 + 2);
                je_cache_binary_list_t list;
                if (!::je_cache_binary_document_index(
                    document, value_key, JE_CACHE_BINARY_VALUE_LIST, &list))
                    return false;

                std::array<float, 4> values{ 0.f, 0.f, 0.f, 0.f };
                if (!read_cache_float_list(&list, values.data(), count))
                    return false;

                if (count == 2)
                    *out = property_value{
                        std::array<float, 2>{ values[0], values[1] } };
                else if (count == 3)
                    *out = property_value{
                        std::array<float, 3>{ values[0], values[1], values[2] } };
                else
                    *out = property_value{ std::array<float, 4>{
                        values[0], values[1], values[2], values[3] } };
                break;
            }
            }

            if (out_type != nullptr)
                *out_type = (je_property_type_t)type;
            return true;
        }

        std::string resolve_runtime_prefix(const std::string& path)
        {
            if (!path.empty() && path[0] == '@')
                return std::string(::je_file_runtime_path_get()) + path.substr(1);
            if (!path.empty() && path[0] == '!')
                return std::string(::je_file_host_path_get()) + path.substr(1);
            return path;
        }

        // ==================== 源网格跟随纹理尺寸 ====================
        bool reconcile_source_grid(TilesetDocument& doc, int32_t source_idx)
        {
            if (source_idx < 0 || source_idx >= (int32_t)doc.sources.size())
                return false;
            auto& src = doc.sources[source_idx];
            if (src.path.empty() || src.tile_px <= 0)
                return false;
            auto tex = graphic::texture2d::load(nullptr, src.path);
            if (!tex.has_value())
                return false;
            const int32_t tw = (int32_t)tex.value()->resource()->m_width;
            const int32_t th = (int32_t)tex.value()->resource()->m_height;
            if (tw < src.tile_px || th < src.tile_px)
            {
                debug::logwarn(
                    "Tilemap: texture '%s' (%dx%d) is smaller than one tile "
                    "(%d px), recorded grid is kept.",
                    src.path.c_str(), tw, th, src.tile_px);
                return false;
            }
            const int32_t nx = tw / src.tile_px;
            const int32_t ny = th / src.tile_px;

            // 附加层级与主网格共用格数：同步重新测量（分辨率可与主纹理
            // 不同，cell 尺寸按各自纹理推得，见 build_extra_array）
            bool extras_changed = false;
            for (auto& extra : src.extras)
            {
                auto etex = graphic::texture2d::load(nullptr, extra.path);
                if (!etex.has_value())
                    continue;
                const int32_t ew = (int32_t)etex.value()->resource()->m_width;
                const int32_t eh = (int32_t)etex.value()->resource()->m_height;
                if (ew / nx < 1 || eh / ny < 1)
                {
                    debug::logwarn(
                        "Tilemap: extra texture '%s' (%dx%d) too small for "
                        "grid %dx%d, recorded size is kept.",
                        extra.path.c_str(), ew, eh, nx, ny);
                    continue;
                }
                if (extra.tex_w != ew || extra.tex_h != eh)
                {
                    extra.tex_w = ew;
                    extra.tex_h = eh;
                    extras_changed = true;
                }
            }

            if (nx == src.xcount && ny == src.ycount)
            {
                // 网格未变：只刷新测量尺寸（UV 换算用）
                if (src.tex_w == tw && src.tex_h == th && !extras_changed)
                    return false;
                src.tex_w = tw;
                src.tex_h = th;
                src.array.reset();
                src.array_failed = false;
                for (auto& extra : src.extras)
                {
                    extra.array.reset();
                    extra.array_failed = false;
                }
                ++doc.version;
                return true;
            }

            const int32_t keep_x = std::min(src.xcount, nx);
            const int32_t keep_y = std::min(src.ycount, ny);
            const int64_t new_cells = (int64_t)nx * ny;
            int64_t occupied = 0;
            for (int32_t iy = 0; iy < keep_y; ++iy)
                for (int32_t ix = 0; ix < keep_x; ++ix)
                {
                    const size_t old_idx = (size_t)iy * src.xcount + ix;
                    if (old_idx < src.cell_ids.size() && src.cell_ids[old_idx] > 0)
                        ++occupied;
                }
            if ((int64_t)doc.next_tile_id + (new_cells - occupied)
                > (int64_t)TILE_ID_BASE)
            {
                debug::logerr(
                    "Tilemap: too many tiles in tileset (limit %d), "
                    "source '%s' keeps its recorded grid.",
                    TILE_ID_BASE, src.path.c_str());
                return false;
            }

            // 被裁掉的格：其瓦片 id 与稀疏元数据一并丢弃
            for (int32_t iy = 0; iy < src.ycount; ++iy)
                for (int32_t ix = 0; ix < src.xcount; ++ix)
                {
                    if (ix < keep_x && iy < keep_y)
                        continue;
                    const size_t old_idx = (size_t)iy * src.xcount + ix;
                    if (old_idx < src.cell_ids.size() && src.cell_ids[old_idx] > 0)
                        doc.tiles.erase(src.cell_ids[old_idx]);
                }

            std::vector<int32_t> ids((size_t)new_cells, 0);
            for (int32_t iy = 0; iy < keep_y; ++iy)
                for (int32_t ix = 0; ix < keep_x; ++ix)
                {
                    const size_t old_idx = (size_t)iy * src.xcount + ix;
                    if (old_idx < src.cell_ids.size())
                        ids[(size_t)iy * nx + ix] = src.cell_ids[old_idx];
                }
            for (int32_t iy = 0; iy < ny; ++iy)
                for (int32_t ix = 0; ix < nx; ++ix)
                {
                    int32_t& id = ids[(size_t)iy * nx + ix];
                    if (id <= 0)
                        id = doc.next_tile_id++;
                }

            debug::loginfo(
                "Tilemap: source '%s' grid follows texture size: %dx%d -> %dx%d.",
                src.path.c_str(), src.xcount, src.ycount, nx, ny);
            src.cell_ids = std::move(ids);
            src.xcount = nx;
            src.ycount = ny;
            src.tex_w = tw;
            src.tex_h = th;
            // 数组纹理按新网格重建（层数与层尺寸均已改变）
            src.array.reset();
            src.array_failed = false;
            for (auto& extra : src.extras)
            {
                extra.array.reset();
                extra.array_failed = false;
            }
            doc.rebuild_tile_cells();
            // blob47 显式变体表可能引用被裁掉的格：剔除失效项（空表回落
            // 到锚点起行优先的默认布局）
            for (auto& entry : doc.tiles)
            {
                auto& variants = entry.second.variant_tiles;
                variants.erase(std::remove_if(variants.begin(), variants.end(),
                    [&](int32_t vid) { return !doc.tile_cell(vid).has_value(); }),
                    variants.end());
            }
            ++doc.version;
            return true;
        }

        // ==================== 文档加载 ====================
        std::optional<std::shared_ptr<TilesetDocument>> load_tileset_from(const std::string& path)
        {
            je_file_t* cache = je_tileset_cache_open_or_create(path.c_str());
            if (cache == nullptr)
                return std::nullopt;

            cache_file_holder holder(cache);

            je_cache_binary_document_t root;
            if (!je_cache_binary_document_open(cache, &root))
                return std::nullopt;

            auto ts = std::make_shared<TilesetDocument>();
            ts->path = path;
            if (!read_cache_string(&root, "name", &ts->name)
                || !read_cache_int32(&root, "next_tile_id", &ts->next_tile_id))
                return std::nullopt;
            if (ts->next_tile_id <= 0 || ts->next_tile_id > TILE_ID_BASE)
                return std::nullopt;

            je_cache_binary_list_t sources;
            if (!::je_cache_binary_document_index(&root, "sources",
                JE_CACHE_BINARY_VALUE_LIST, &sources))
                return std::nullopt;

            je_cache_binary_list_iterator_t source_it;
            if (!::je_cache_binary_list_iterator_open(&sources, &source_it))
                return std::nullopt;

            while (!::je_cache_binary_list_iterator_is_end(&source_it))
            {
                je_cache_binary_document_t source_doc;
                if (!::je_cache_binary_list_iterator_read(&source_it,
                    JE_CACHE_BINARY_VALUE_DOCUMENT, &source_doc))
                    return std::nullopt;

                SourceTexture s;
                int32_t tw = 0, th = 0;
                if (!read_cache_string(&source_doc, "path", &s.path)
                    || !read_cache_int32(&source_doc, "tile_px", &s.tile_px)
                    || !read_cache_int32(&source_doc, "xcount", &s.xcount)
                    || !read_cache_int32(&source_doc, "ycount", &s.ycount)
                    || !read_cache_int32(&source_doc, "tex_w", &tw)
                    || !read_cache_int32(&source_doc, "tex_h", &th))
                    return std::nullopt;
                if (tw > 0) s.tex_w = tw;
                if (th > 0) s.tex_h = th;
                if (s.tile_px <= 0 || s.xcount <= 0 || s.ycount <= 0
                    || (int64_t)s.xcount * s.ycount > (int64_t)TILE_ID_BASE)
                    return std::nullopt;

                const int64_t cell_count = (int64_t)s.xcount * s.ycount;
                if (!read_cache_int32_buffer(&source_doc, "cell_ids",
                    cell_count, &s.cell_ids))
                    return std::nullopt;
                for (int32_t id : s.cell_ids)
                    if (id <= 0 || id >= ts->next_tile_id
                        || id >= (int32_t)TILE_ID_BASE)
                        return std::nullopt;

                je_cache_binary_list_t extras;
                if (!::je_cache_binary_document_index(&source_doc, "extras",
                    JE_CACHE_BINARY_VALUE_LIST, &extras))
                    return std::nullopt;

                je_cache_binary_list_iterator_t extra_it;
                if (!::je_cache_binary_list_iterator_open(&extras, &extra_it))
                    return std::nullopt;

                while (!::je_cache_binary_list_iterator_is_end(&extra_it))
                {
                    je_cache_binary_document_t extra_doc;
                    if (!::je_cache_binary_list_iterator_read(&extra_it,
                        JE_CACHE_BINARY_VALUE_DOCUMENT, &extra_doc))
                        return std::nullopt;

                    SourceExtra e;
                    int32_t ew = 0, eh = 0;
                    if (!read_cache_int32(&extra_doc, "pass", &e.pass)
                        || !read_cache_string(&extra_doc, "path", &e.path)
                        || !read_cache_int32(&extra_doc, "tex_w", &ew)
                        || !read_cache_int32(&extra_doc, "tex_h", &eh))
                        return std::nullopt;
                    if (ew > 0) e.tex_w = ew;
                    if (eh > 0) e.tex_h = eh;
                    if (e.pass < 1 || e.path.empty())
                        return std::nullopt;
                    for (const auto& prev : s.extras)
                        if (prev.pass == e.pass)
                            return std::nullopt;
                    s.extras.push_back(std::move(e));

                    if (!::je_cache_binary_list_iterator_advance(&extra_it))
                        return std::nullopt;
                }

                ts->sources.push_back(std::move(s));

                if (!::je_cache_binary_list_iterator_advance(&source_it))
                    return std::nullopt;
            }

            // 建立 id -> 格 反向索引；逐格 id 须全局唯一（重复即损坏）
            ts->rebuild_tile_cells();
            int64_t cells_total = 0;
            for (const auto& s : ts->sources)
                cells_total += (int64_t)s.xcount * s.ycount;
            if ((int64_t)ts->tile_cells.size() != cells_total)
                return std::nullopt;

            je_cache_binary_list_t properties;
            if (!::je_cache_binary_document_index(&root, "properties",
                JE_CACHE_BINARY_VALUE_LIST, &properties))
                return std::nullopt;

            je_cache_binary_list_iterator_t property_it;
            if (!::je_cache_binary_list_iterator_open(&properties, &property_it))
                return std::nullopt;

            while (!::je_cache_binary_list_iterator_is_end(&property_it))
            {
                je_cache_binary_document_t property_doc;
                if (!::je_cache_binary_list_iterator_read(&property_it,
                    JE_CACHE_BINARY_VALUE_DOCUMENT, &property_doc))
                    return std::nullopt;

                PropertyDef pd;
                int32_t type = 0;
                if (!read_cache_string(&property_doc, "name", &pd.name)
                    || !read_cache_int32(&property_doc, "type", &type)
                    || !property_type_valid(type))
                    return std::nullopt;
                pd.type = (je_property_type_t)type;

                je_property_type_t value_type = JE_PROPERTY_TYPE_BOOL;
                if (!read_cache_property_value(
                    &property_doc, "default", &pd.default_value, &value_type)
                    || value_type != pd.type)
                    return std::nullopt;

                ts->properties.push_back(std::move(pd));

                if (!::je_cache_binary_list_iterator_advance(&property_it))
                    return std::nullopt;
            }

            je_cache_binary_list_t tiles;
            if (!::je_cache_binary_document_index(&root, "tiles",
                JE_CACHE_BINARY_VALUE_LIST, &tiles))
                return std::nullopt;

            je_cache_binary_list_iterator_t tile_it;
            if (!::je_cache_binary_list_iterator_open(&tiles, &tile_it))
                return std::nullopt;

            while (!::je_cache_binary_list_iterator_is_end(&tile_it))
            {
                je_cache_binary_document_t tile_doc;
                if (!::je_cache_binary_list_iterator_read(&tile_it,
                    JE_CACHE_BINARY_VALUE_DOCUMENT, &tile_doc))
                    return std::nullopt;

                int32_t tile_id = 0;
                TileDef t;
                int32_t kind = 0;
                if (!read_cache_int32(&tile_doc, "id", &tile_id)
                    || !read_cache_string(&tile_doc, "name", &t.name)
                    || !read_cache_bool(&tile_doc, "walkable", &t.walkable)
                    || !read_cache_int32(&tile_doc, "kind", &kind))
                    return std::nullopt;
                if (!ts->tile_cell(tile_id).has_value()
                    || kind < 0 || kind > (int32_t)JE_TILE_KIND_RPGMAKER_VX)
                    return std::nullopt;
                t.kind = (je_tile_kind_t)kind;

                je_cache_binary_list_t normal;
                if (!::je_cache_binary_document_index(&tile_doc, "normal",
                    JE_CACHE_BINARY_VALUE_LIST, &normal))
                    return std::nullopt;
                float normal_values[3] = { 0.f, 0.f, 0.f };
                if (!read_cache_float_list(&normal, normal_values, 3))
                    return std::nullopt;

                // 顶点法线按文件值归一化；零长或含非有限分量视为损坏
                const float nlen = std::sqrt(normal_values[0] * normal_values[0]
                    + normal_values[1] * normal_values[1]
                    + normal_values[2] * normal_values[2]);
                if (!std::isfinite(nlen) || nlen <= 0.f)
                    return std::nullopt;
                t.normal = { normal_values[0] / nlen,
                    normal_values[1] / nlen, normal_values[2] / nlen };

                if (!read_cache_int32_buffer(
                    &tile_doc, "variants", -1, &t.variant_tiles))
                    return std::nullopt;
                for (int32_t variant : t.variant_tiles)
                    if (!ts->tile_cell(variant).has_value())
                        return std::nullopt;

                je_cache_binary_list_t tile_properties;
                if (!::je_cache_binary_document_index(&tile_doc, "properties",
                    JE_CACHE_BINARY_VALUE_LIST, &tile_properties))
                    return std::nullopt;

                je_cache_binary_list_iterator_t tile_property_it;
                if (!::je_cache_binary_list_iterator_open(
                    &tile_properties, &tile_property_it))
                    return std::nullopt;

                while (!::je_cache_binary_list_iterator_is_end(
                    &tile_property_it))
                {
                    je_cache_binary_document_t property_doc;
                    if (!::je_cache_binary_list_iterator_read(
                        &tile_property_it,
                        JE_CACHE_BINARY_VALUE_DOCUMENT, &property_doc))
                        return std::nullopt;

                    std::string name;
                    property_value value;
                    if (!read_cache_string(&property_doc, "name", &name)
                        || !read_cache_property_value(
                            &property_doc, "value", &value, nullptr))
                        return std::nullopt;
                    t.properties[std::move(name)] = std::move(value);

                    if (!::je_cache_binary_list_iterator_advance(
                        &tile_property_it))
                        return std::nullopt;
                }

                ts->tiles[tile_id] = std::move(t);

                if (!::je_cache_binary_list_iterator_advance(&tile_it))
                    return std::nullopt;
            }

            // 网格跟随纹理当前尺寸：纹理增删一行/一列时图集随之增删
            // 一行/一列瓦片（已有瓦片 id 不变，新增格从末尾追加新 id）
            for (size_t i = 0; i < ts->sources.size(); ++i)
                reconcile_source_grid(*ts, (int32_t)i);
            return ts;
        }

        bool apply_cache_uniform(graphic::shader* shader,
            const je_cache_binary_document_t* document, int32_t type)
        {
            std::string name;
            if (!read_cache_string(document, "name", &name))
                return false;

            je_cache_binary_list_t list;
            int32_t int_values[4] = { 0, 0, 0, 0 };
            float float_values[4] = { 0.f, 0.f, 0.f, 0.f };

            const auto read_int_list = [&](size_t count)
            {
                return ::je_cache_binary_document_index(document, "value",
                           JE_CACHE_BINARY_VALUE_LIST, &list)
                    && read_cache_int32_list(&list, int_values, count);
            };
            const auto read_float_list_value = [&](size_t count)
            {
                return ::je_cache_binary_document_index(document, "value",
                           JE_CACHE_BINARY_VALUE_LIST, &list)
                    && read_cache_float_list(&list, float_values, count);
            };

            switch (type)
            {
            case 0:
                if (!read_cache_int32(document, "value", &int_values[0]))
                    return false;
                shader->set_uniform(name, int_values[0]);
                break;
            case 1:
                if (!read_int_list(2))
                    return false;
                shader->set_uniform(name, int_values[0], int_values[1]);
                break;
            case 2:
                if (!read_int_list(3))
                    return false;
                shader->set_uniform(name,
                    int_values[0], int_values[1], int_values[2]);
                break;
            case 3:
                if (!read_int_list(4))
                    return false;
                shader->set_uniform(name,
                    int_values[0], int_values[1], int_values[2], int_values[3]);
                break;
            case 4:
                if (!read_cache_float32(document, "value", &float_values[0]))
                    return false;
                shader->set_uniform(name, float_values[0]);
                break;
            case 5:
                if (!read_float_list_value(2))
                    return false;
                shader->set_uniform(name,
                    math::vec2(float_values[0], float_values[1]));
                break;
            case 6:
                if (!read_float_list_value(3))
                    return false;
                shader->set_uniform(name, math::vec3(float_values[0],
                    float_values[1], float_values[2]));
                break;
            case 7:
                if (!read_float_list_value(4))
                    return false;
                shader->set_uniform(name, math::vec4(float_values[0],
                    float_values[1], float_values[2], float_values[3]));
                break;
            default:
                return false;
            }
            return true;
        }

        std::optional<std::shared_ptr<MapDocument>> load_map_from(const std::string& path)
        {
            je_file_t* cache = je_tilemap_cache_open_or_create(path.c_str());
            if (cache == nullptr)
                return std::nullopt;

            cache_file_holder holder(cache);

            je_cache_binary_document_t root;
            if (!je_cache_binary_document_open(cache, &root))
                return std::nullopt;

            auto m = std::make_shared<MapDocument>();
            m->path = path;
            if (!read_cache_int32(&root, "width", &m->width)
                || !read_cache_int32(&root, "height", &m->height)
                || !read_cache_int32(&root, "tile_px", &m->tile_px))
                return std::nullopt;
            if (m->width < 0 || m->height < 0 || m->tile_px <= 0
                || m->width > 65536 || m->height > 65536)
                return std::nullopt;

            je_cache_binary_list_t tilesets;
            if (!::je_cache_binary_document_index(&root, "tilesets",
                JE_CACHE_BINARY_VALUE_LIST, &tilesets))
                return std::nullopt;

            je_cache_binary_list_iterator_t tileset_it;
            if (!::je_cache_binary_list_iterator_open(&tilesets, &tileset_it))
                return std::nullopt;

            while (!::je_cache_binary_list_iterator_is_end(&tileset_it))
            {
                je_cache_binary_string_t tileset_path;
                if (!::je_cache_binary_list_iterator_read(&tileset_it,
                    JE_CACHE_BINARY_VALUE_STRING, &tileset_path))
                    return std::nullopt;

                std::string resolved;
                if (!read_cache_string_value(&tileset_path, &resolved))
                    return std::nullopt;
                m->tilesets.push_back(
                    TilesetRef{ std::move(resolved), std::nullopt });

                if (!::je_cache_binary_list_iterator_advance(&tileset_it))
                    return std::nullopt;
            }

            je_cache_binary_list_t layers;
            if (!::je_cache_binary_document_index(&root, "layers",
                JE_CACHE_BINARY_VALUE_LIST, &layers))
                return std::nullopt;

            je_cache_binary_list_iterator_t layer_it;
            if (!::je_cache_binary_list_iterator_open(&layers, &layer_it))
                return std::nullopt;

            while (!::je_cache_binary_list_iterator_is_end(&layer_it))
            {
                je_cache_binary_document_t layer_doc;
                if (!::je_cache_binary_list_iterator_read(&layer_it,
                    JE_CACHE_BINARY_VALUE_DOCUMENT, &layer_doc))
                    return std::nullopt;

                MapLayer l;
                int32_t selfshadow = 0, blockshadow = 0;
                if (!read_cache_string(&layer_doc, "name", &l.name)
                    || !read_cache_bool(&layer_doc, "visible", &l.visible)
                    || !read_cache_bool(&layer_doc, "locked", &l.locked)
                    || !read_cache_float32(&layer_doc, "z", &l.z)
                    || !read_cache_int32(&layer_doc, "rend_queue", &l.rend_queue)
                    || !read_cache_int32(&layer_doc, "selfshadow", &selfshadow)
                    || !read_cache_float32(&layer_doc, "selfshadow_factor",
                        &l.selfshadow_factor)
                    || !read_cache_int32(&layer_doc, "blockshadow", &blockshadow)
                    || !read_cache_float32(&layer_doc, "blockshadow_factor",
                        &l.blockshadow_factor))
                    return std::nullopt;
                if (selfshadow < 0
                    || selfshadow > (int32_t)JE_LAYER_SELFSHADOW_AUTO
                    || blockshadow < 0
                    || blockshadow > (int32_t)JE_LAYER_BLOCKSHADOW_AUTO)
                    return std::nullopt;
                l.selfshadow = (je_layer_self_shadow_t)selfshadow;
                l.blockshadow = (je_layer_block_shadow_t)blockshadow;

                if (!read_cache_int32_buffer(&layer_doc, "grid",
                    (int64_t)m->width * m->height, &l.grid))
                    return std::nullopt;

                // 图层样式：着色器/附加纹理按来源路径加载；加载失败只
                // 告警并按未配置处理（不拒绝文档）。uniform 值写回实例。
                std::string shader_path;
                if (!read_cache_string(&layer_doc, "shader", &shader_path))
                    return std::nullopt;

                graphic::shader* shader = nullptr;
                if (!shader_path.empty())
                {
                    if (auto loaded = graphic::shader::load(nullptr, shader_path))
                    {
                        l.style.shader = loaded;
                        shader = l.style.shader.value().get();
                    }
                    else
                        debug::logerr(
                            "Tilemap: unable to load layer shader '%s'.",
                            shader_path.c_str());
                }

                je_cache_binary_list_t textures;
                if (!::je_cache_binary_document_index(&layer_doc, "textures",
                    JE_CACHE_BINARY_VALUE_LIST, &textures))
                    return std::nullopt;

                je_cache_binary_list_iterator_t texture_it;
                if (!::je_cache_binary_list_iterator_open(
                    &textures, &texture_it))
                    return std::nullopt;

                while (!::je_cache_binary_list_iterator_is_end(&texture_it))
                {
                    je_cache_binary_document_t texture_doc;
                    if (!::je_cache_binary_list_iterator_read(&texture_it,
                        JE_CACHE_BINARY_VALUE_DOCUMENT, &texture_doc))
                        return std::nullopt;

                    int32_t slot = 0;
                    std::string texture_path;
                    if (!read_cache_int32(&texture_doc, "slot", &slot)
                        || !read_cache_string(&texture_doc, "path", &texture_path))
                        return std::nullopt;
                    if (slot < 1)
                        return std::nullopt;

                    if (auto texture =
                        graphic::texture2d::load(nullptr, texture_path))
                        l.style.textures.push_back(
                            LayerTexture{ slot, std::move(texture.value()) });
                    else
                        debug::logerr(
                            "Tilemap: unable to load layer texture '%s'.",
                            texture_path.c_str());

                    if (!::je_cache_binary_list_iterator_advance(&texture_it))
                        return std::nullopt;
                }

                je_cache_binary_list_t uniforms;
                if (!::je_cache_binary_document_index(&layer_doc, "uniforms",
                    JE_CACHE_BINARY_VALUE_LIST, &uniforms))
                    return std::nullopt;

                je_cache_binary_list_iterator_t uniform_it;
                if (!::je_cache_binary_list_iterator_open(
                    &uniforms, &uniform_it))
                    return std::nullopt;

                while (!::je_cache_binary_list_iterator_is_end(&uniform_it))
                {
                    je_cache_binary_document_t uniform_doc;
                    if (!::je_cache_binary_list_iterator_read(&uniform_it,
                        JE_CACHE_BINARY_VALUE_DOCUMENT, &uniform_doc))
                        return std::nullopt;

                    int32_t type = 0;
                    if (!read_cache_int32(&uniform_doc, "type", &type)
                        || type < 0 || type > 7)
                        return std::nullopt;

                    if (shader != nullptr
                        && !apply_cache_uniform(shader, &uniform_doc, type))
                        return std::nullopt;

                    if (!::je_cache_binary_list_iterator_advance(&uniform_it))
                        return std::nullopt;
                }

                m->layers.push_back(std::move(l));

                if (!::je_cache_binary_list_iterator_advance(&layer_it))
                    return std::nullopt;
            }

            je_cache_binary_list_t cell_properties;
            if (!::je_cache_binary_document_index(&root, "cell_properties",
                JE_CACHE_BINARY_VALUE_LIST, &cell_properties))
                return std::nullopt;

            je_cache_binary_list_iterator_t cell_it;
            if (!::je_cache_binary_list_iterator_open(
                &cell_properties, &cell_it))
                return std::nullopt;

            while (!::je_cache_binary_list_iterator_is_end(&cell_it))
            {
                je_cache_binary_document_t cell_doc;
                if (!::je_cache_binary_list_iterator_read(&cell_it,
                    JE_CACHE_BINARY_VALUE_DOCUMENT, &cell_doc))
                    return std::nullopt;

                int32_t layer = 0, x = 0, y = 0;
                if (!read_cache_int32(&cell_doc, "layer", &layer)
                    || !read_cache_int32(&cell_doc, "x", &x)
                    || !read_cache_int32(&cell_doc, "y", &y))
                    return std::nullopt;

                je_cache_binary_list_t properties;
                if (!::je_cache_binary_document_index(&cell_doc, "properties",
                    JE_CACHE_BINARY_VALUE_LIST, &properties))
                    return std::nullopt;

                je_cache_binary_list_iterator_t property_it;
                if (!::je_cache_binary_list_iterator_open(
                    &properties, &property_it))
                    return std::nullopt;

                std::map<std::string, property_value> values;
                while (!::je_cache_binary_list_iterator_is_end(&property_it))
                {
                    je_cache_binary_document_t property_doc;
                    if (!::je_cache_binary_list_iterator_read(&property_it,
                        JE_CACHE_BINARY_VALUE_DOCUMENT, &property_doc))
                        return std::nullopt;

                    std::string name;
                    property_value value;
                    if (!read_cache_string(&property_doc, "name", &name)
                        || !read_cache_property_value(
                            &property_doc, "value", &value, nullptr))
                        return std::nullopt;
                    values[std::move(name)] = std::move(value);

                    if (!::je_cache_binary_list_iterator_advance(&property_it))
                        return std::nullopt;
                }

                if (m->in_bounds(layer, x, y))
                    m->cell_properties[{ layer, x + y * m->width }] =
                        std::move(values);
                else
                    debug::logwarn(
                        "Tilemap: cell property entry for out-of-range cell "
                        "(%d,%d) on layer %d is ignored.",
                        x, y, layer);

                if (!::je_cache_binary_list_iterator_advance(&cell_it))
                    return std::nullopt;
            }

            return m;
        }

        // ==================== 文档注册表 ====================
        int32_t documents::map_count() const
        {
            std::lock_guard lock(m_mutex);
            return (int32_t)m_maps.all.size();
        }

        std::optional<std::shared_ptr<MapDocument>> documents::map_at(int32_t index) const
        {
            std::lock_guard lock(m_mutex);
            if (index < 0 || index >= (int32_t)m_maps.all.size())
                return std::nullopt;
            return live(m_maps.all[(size_t)index]);
        }

        std::vector<MapDocument*> documents::snapshot_maps()
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

        std::optional<std::shared_ptr<MapDocument>> documents::open_map(const std::string& path)
        {
            std::lock_guard lock(m_mutex);
            auto doc = resolve(m_maps, path, &load_map_from, "map");
            if (doc.has_value())
                acquire_ref(m_maps, *doc);
            return doc;
        }

        std::optional<std::shared_ptr<TilesetDocument>> documents::open_tileset(const std::string& path)
        {
            std::lock_guard lock(m_mutex);
            auto doc = resolve(m_tilesets, path, &load_tileset_from, "tileset");
            if (doc.has_value())
                acquire_ref(m_tilesets, *doc);
            return doc;
        }

        std::optional<std::shared_ptr<MapDocument>> documents::observe_map(const std::string& path)
        {
            std::lock_guard lock(m_mutex);
            return resolve(m_maps, path, &load_map_from, "map");
        }

        std::optional<std::shared_ptr<TilesetDocument>> documents::observe_tileset(const std::string& path)
        {
            std::lock_guard lock(m_mutex);
            return resolve(m_tilesets, path, &load_tileset_from, "tileset");
        }

        MapDocument* documents::register_map(std::shared_ptr<MapDocument> doc)
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

        TilesetDocument* documents::register_tileset(std::shared_ptr<TilesetDocument> doc)
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

        bool documents::bind_map_path(MapDocument* doc, const std::string& path)
        {
            assert(doc != nullptr);
            if (path.empty())
                return false;
            std::lock_guard lock(m_mutex);
            auto fnd = m_maps.held.find(doc);
            if (fnd == m_maps.held.end())
                return false;
            rebind_path(m_maps, doc->path, path, fnd->second.hold);
            doc->path = path;
            return true;
        }

        bool documents::bind_tileset_path(TilesetDocument* doc, const std::string& path)
        {
            assert(doc != nullptr);
            if (path.empty())
                return false;
            std::lock_guard lock(m_mutex);
            auto fnd = m_tilesets.held.find(doc);
            if (fnd == m_tilesets.held.end())
                return false;
            rebind_path(m_tilesets, doc->path, path, fnd->second.hold);
            doc->path = path;
            return true;
        }

        void documents::reload_map(const std::string& path)
        {
            std::lock_guard lock(m_mutex);
            detach(m_maps, canonical_key(path));
        }

        void documents::reload_tileset(const std::string& path)
        {
            std::lock_guard lock(m_mutex);
            detach(m_tilesets, canonical_key(path));
        }

        int32_t documents::keep_map(MapDocument* doc)
        {
            if (doc == nullptr)
                return 0;
            std::lock_guard lock(m_mutex);
            auto fnd = m_maps.held.find(doc);
            return fnd == m_maps.held.end() ? 0 : ++fnd->second.refs;
        }

        int32_t documents::keep_tileset(TilesetDocument* doc)
        {
            if (doc == nullptr)
                return 0;
            std::lock_guard lock(m_mutex);
            auto fnd = m_tilesets.held.find(doc);
            return fnd == m_tilesets.held.end() ? 0 : ++fnd->second.refs;
        }

        int32_t documents::unload_map(MapDocument* doc)
        {
            if (doc == nullptr)
                return 0;
            std::lock_guard lock(m_mutex);
            return release_ref(m_maps, doc);
        }

        int32_t documents::unload_tileset(TilesetDocument* doc)
        {
            if (doc == nullptr)
                return 0;
            std::lock_guard lock(m_mutex);
            return release_ref(m_tilesets, doc);
        }

        void documents::ensure_tilesets(MapDocument& m)
        {
            std::lock_guard lock(m_mutex);
            for (auto& ts : m.tilesets)
                ts.resolved = resolve(m_tilesets, ts.path,
                    &load_tileset_from, "tileset");
        }

        void documents::append_tileset_entry(MapDocument& m, const std::string& path)
        {
            std::lock_guard lock(m_mutex);
            m.tilesets.push_back(TilesetRef{ path, std::nullopt });
        }

        void documents::erase_tileset_entry(MapDocument& m, size_t idx)
        {
            std::lock_guard lock(m_mutex);
            if (idx >= m.tilesets.size())
                return;
            m.tilesets.erase(m.tilesets.begin() + idx);
        }

        uint64_t documents::map_stamp(const MapDocument& m) const
        {
            std::lock_guard lock(m_mutex);
            uint64_t stamp = mix(0x9E3779B97F4A7C15ull, (uintptr_t)&m, m.version);
            for (const auto& ts : m.tilesets)
                stamp = mix(stamp,
                    ts.resolved ? (uintptr_t)ts.resolved->get() : 0,
                    ts.resolved ? (*ts.resolved)->version : 0);
            return stamp;
        }

        uint64_t documents::mix(uint64_t h, uintptr_t identity, uint64_t version)
        {
            return (h ^ (uint64_t)identity) * 0x100000001B3ull ^ (version * 31 + 0x2545F491);
        }

        std::string documents::canonical_key(const std::string& path) const
        {
            std::string s = resolve_runtime_prefix(path);
            bool absolute = s.size() >= 2 && (s[1] == ':' || s[0] == '/');
            if (!absolute)
                s = std::string(::je_file_runtime_path_get()) + '/' + s;
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

        // ==================== 自动图块 ====================
        int compute_mask(const MapDocument& m, int32_t layer, int32_t x, int32_t y, int32_t encoded)
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

        half_cell rpgmaker_quadrant(int corner /*0=LU 1=RU 2=LD 3=RD*/, int state)
        {
            static constexpr int TABLE[4][5][2] = {
                /*LU*/ { {2,4},{2,0},{0,4},{2,2},{0,2} },
                /*RU*/ { {1,4},{3,0},{3,4},{1,2},{3,2} },
                /*LD*/ { {2,3},{2,1},{0,3},{2,5},{0,5} },
                /*RD*/ { {1,3},{3,1},{3,3},{1,5},{3,5} },
            };
            return { TABLE[corner][state][0], TABLE[corner][state][1] };
        }

        int corner_state(int mask, int o1, int o2, int diag)
        {
            if ((mask & o1) && (mask & o2))
                return (mask & diag) ? 0 : 1;
            if (mask & o1) return 2;
            if (mask & o2) return 3;
            return 4;
        }

        const std::vector<uint8_t>& blob47_table()
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
        const property_value* resolve_property(
            MapDocument* doc, int32_t layer, int32_t x, int32_t y, const std::string& name)
        {
            if (doc == nullptr || !doc->in_bounds(layer, x, y))
                return nullptr;
            // API 直调时无系统代管图集解析，此处兜底（幂等）
            documents::inst().ensure_tilesets(*doc);

            return resolve_property_resolved(doc, layer, x, y, name);
        }
        const property_value* resolve_property_resolved(
            MapDocument* doc, int32_t layer, int32_t x, int32_t y, const std::string& name)
        {
            if (doc == nullptr || !doc->in_bounds(layer, x, y))
                return nullptr;
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

        bool walkable_on(MapDocument* doc, int32_t layer, int32_t x, int32_t y)
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

        // ==================== 单元格渲染解析 ====================
        std::optional<basic::resource<graphic::texture_array>>
            build_grid_array(const std::string& path,
                int32_t xcount, int32_t ycount, int32_t cell_w, int32_t cell_h)
        {
            auto tex = graphic::texture2d::load(nullptr, path);
            if (!tex.has_value())
                return std::nullopt;
            auto* raw = tex.value()->resource();
            if (raw->m_initial_pixels == nullptr
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
            const size_t layer_count = (size_t)(xcount * ycount);

            // 先在 CPU 侧构建完整像素快照，再交给数组纹理创建
            std::vector<uint8_t> layer_pixels(
                layer_count * cw * ch * 4, 0);
            uint8_t* dst = layer_pixels.data();

            for (int32_t ty = 0; ty < yc; ++ty)
            {
                for (int32_t tx = 0; tx < xc; ++tx)
                {
                    uint8_t* dst_layer =
                        dst + (size_t)((ty * xcount + tx) * cell_w * cell_h) * 4;
                    // 2D 纹理经 stbi_set_flip_vertically_on_load 加载，
                    // m_initial_pixels 为自底向上行序（缓冲行 0 = 图像底行），
                    // 瓦片 (tx,ty) 在缓冲中位于行 [th-(ty+1)*ch, th-ty*ch)。
                    // 层行序沿用该翻转序（层行 0 = 瓦片底行，采样 v=1 =
                    // 瓦片顶），与 tile-local UV 及编辑器换算的 v=1 为顶
                    // 约定一致。
                    const size_t buf_row0 = (size_t)(th - (ty + 1) * cell_h);
                    for (size_t r = 0; r < ch; ++r)
                        memcpy(
                            dst_layer + r * cw * 4,
                            raw->m_initial_pixels
                                + ((buf_row0 + r) * (size_t)tw
                                    + (size_t)(tx * cell_w)) * 4,
                            cw * 4);
                }
            }

            return graphic::texture_array::create(
                cw, ch, layer_count,
                (je_gl_texture_format_t)(
                    JE_GL_TEXTURE_FORMAT_RGBA
                    | (raw->m_format & JE_GL_TEXTURE_FORMAT_SRGB)),
                layer_pixels.data());
        }

        bool build_source_array(SourceTexture& src)
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

        bool build_extra_array(SourceTexture& src, SourceExtra& extra)
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

        void tile_uv(const SourceTexture& src, int32_t ix, int32_t iy,
            float& u0, float& v0_bot, float& u1, float& v1_top, int32_t& layer)
        {
            u0 = 0.f;
            u1 = 1.f;
            v1_top = 1.f;
            v0_bot = 0.f;
            layer = iy * src.xcount + ix;
        }

        int32_t resolve_cell_quads(MapDocument& doc, int32_t layer,
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
            const je_tile_kind_t kind = ts->tile_kind(tile_id);
            const TileDef* def = ts->tile_def(tile_id);
            // 顶点法线取锚点瓦片（地图格存储的瓦片）的元数据；自动拼接
            // 解析出的变体/象限只是该格的取样来源，不覆盖法线
            const std::array<float, 3> normal =
                def == nullptr ? DEFAULT_TILE_NORMAL : def->normal;

            // 整格取件：按原样绘制 / 普通瓦片 / blob47 变体
            auto fill_full = [&](int32_t src_idx, int32_t cx, int32_t cy)
            {
                out[0].tileset_idx = ts_idx;
                out[0].source_idx = src_idx;
                tile_uv(ts->sources[src_idx], cx, cy,
                    out[0].u0, out[0].v0, out[0].u1, out[0].v1, out[0].layer);
                out[0].normal = normal;
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
                    out[c].normal = normal;
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
