#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_core_tilemap_system.hpp"
#include "jeecs_core_editor_components.hpp"

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

namespace jeecs::tilemap
{
    using namespace slice_requirement;
    using namespace Renderer;
    using namespace Transform;

    namespace
    {
        constexpr size_t CHUNK = 32;         // 分块边长（格）
        // pos3 + uv3(u,v,layer) + normal3 + bottom1：
        // normal 取自瓦片元数据（每块瓦片可指定，默认 (0,0,-1) 指向屏幕
        // 外侧）；bottom 为本块底边局部 Y（Light2D::PerBlockSelfShadow
        // 契约，TopDown 光源的逐块自阴影高度判定用；尾部追加对 ≤3 属性
        // 的着色器不可见——顶点属性按网格布局前缀绑定）
        constexpr size_t FLOATS_PER_VERTEX = 10;
        constexpr size_t VERTS_PER_CELL = 16;     // 每格 4 个子四边形槽位
        constexpr size_t FLOATS_PER_CELL = VERTS_PER_CELL * FLOATS_PER_VERTEX;

        // ---- 共享资源 ----

        // 分片着色器与期望一致则不动，否则重写（资源句柄不同即更新）。
        // 着色器唯一来源是图层样式：未配置样式的层清空分片着色器，
        // 不回落地图根实体或内置默认（编辑器建图/建层时逐层指配）
        void refresh_part_shader(const Tilemap::LayerStyle* style,
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
        void refresh_part_textures(const game_entity& part,
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

        // 分片数组纹理对齐：Textures2DArray 承载图集主数组（pass 0）与
        // 源纹理各附加层级的数组（pass >= 1，如图集法线）。图集编辑
        //（增删层级/改通道）与图集重载都不计入地图版本戳，故逐帧与
        // 文档比对重绑（与 refresh_part_textures 同款轻量对齐）。数组
        // 未就绪的层级暂缺，就绪后由比对差异触发重绑。
        void refresh_part_arrays(const game_entity& part,
            const Tilemap::SourceTexture* src)
        {
            auto* comp = part.get_component<Renderer::Textures2DArray>();
            if (comp == nullptr)
                return;

            // 期望绑定序：主数组在前，层级按声明序（与重绑序一致）
            bool matched = true;
            size_t idx = 0;
            if (src != nullptr && src->array.has_value())
            {
                if (idx >= comp->textures.size()
                    || comp->textures[idx].m_pass_id != 0
                    || comp->textures[idx].m_texture.get() != src->array->get())
                    matched = false;
                else
                    ++idx;
            }
            if (matched && src != nullptr)
                for (const auto& e : src->extras)
                {
                    if (!e.array.has_value())
                        continue;
                    if (idx >= comp->textures.size()
                        || comp->textures[idx].m_pass_id != (size_t)e.pass
                        || comp->textures[idx].m_texture.get() != e.array->get())
                    {
                        matched = false;
                        break;
                    }
                    ++idx;
                }
            if (matched && idx == comp->textures.size())
                return;

            // 重绑：按文档重建（未就绪的层级本次留空）
            comp->textures.clear();
            if (src != nullptr)
            {
                if (src->array.has_value())
                    comp->bind_texture(0, *src->array);
                for (const auto& e : src->extras)
                    if (e.array.has_value())
                        comp->bind_texture((size_t)e.pass, *e.array);
            }
        }

        // 比例尺同步：把根实体 LocalScale 复制到分片 LocalScale。
        // 变换系统的缩放为纯局部语义（不沿父链复合，设计如此），
        // 瓦片地图的比例尺因此由本系统显式维护。
        void sync_part_scale(const game_entity& root, const game_entity& part)
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

        // 图层自阴影对齐：OFF 移除分片上的 SelfShadow 组件（不参与
        // 自阴影覆盖），ON/AUTO 挂载并把 auto_disable 对齐到图层模式、
        // factor 对齐到图层强度。瓦片网格顶点携带块底边（bottom 属性），
        // 故同挂 PerBlockSelfShadow——TopDown 光源下 AUTO 高度判定逐格
        // 进行。组件增删走命令缓冲、下一帧才迁移生效，
        // 与着色器/纹理/比例尺同款每帧对齐
        void sync_part_selfshadow(const game_entity& part,
            je_layer_self_shadow_t layer_selfshadow, float selfshadow_factor)
        {
            auto* selfshadow = part.get_component<Light2D::SelfShadow>();
            if (layer_selfshadow == JE_LAYER_SELFSHADOW_OFF)
            {
                if (selfshadow != nullptr)
                    part.remove_component<Light2D::SelfShadow>();
                if (part.get_component<Light2D::PerBlockSelfShadow>() != nullptr)
                    part.remove_component<Light2D::PerBlockSelfShadow>();
                return;
            }
            if (selfshadow == nullptr)
                selfshadow = part.add_component<Light2D::SelfShadow>();
            if (part.get_component<Light2D::PerBlockSelfShadow>() == nullptr)
                part.add_component<Light2D::PerBlockSelfShadow>();
            selfshadow->factor = selfshadow_factor;
            selfshadow->auto_disable =
                layer_selfshadow == JE_LAYER_SELFSHADOW_AUTO;
        }

        // ---- 图层遮挡阴影：分块实心格边界 → 阴影体条带 ----

        // 该格是否遮挡光线：有有效瓦片即实心（与 scan_needed 的格子
        // 有效性同口径——空格/无法解析的格子不遮挡）
        bool cell_blocks_light(const Tilemap::MapDocument& doc,
            const Tilemap::MapLayer& layer, int32_t x, int32_t y)
        {
            if (x < 0 || y < 0 || x >= doc.width || y >= doc.height)
                return false;
            const int32_t v = layer.grid[(size_t)y * doc.width + x];
            if (v == 0)
                return false;
            auto cell = Tilemap::decode_tile_value(v);
            if (!cell.has_value()
                || cell->tileset_idx < 0
                || cell->tileset_idx >= (int32_t)doc.tilesets.size()
                || !doc.tilesets[cell->tileset_idx].resolved.has_value())
                return false;
            const auto* ts = doc.tilesets[cell->tileset_idx].resolved->get();
            return ts->tile_cell(cell->local_id).has_value();
        }

        // 生成分块的遮挡轮廓条带并写入组件的 m_block_mesh。局部空间与
        // 分片渲染网格一致（1 格 = 1 单位，向 +x 与 -y 展开）。实心格
        // 与空心格的边界按行/列贪心合并为轴线段，上/下/左/右四类各自
        // 独立成趟，保证 1 格厚 run 的两侧边界都被发射；绕向与组件缺省
        // 方块一致（实心区域逆时针），配合背面剔除只保留背光边。段与段
        // 以零面积过渡四边形衔接（衔接点对全部取原位因子 0），单条
        // TRIANGLESTRIP 即可承载任意多条不相邻线段。邻居判定取整层
        // 网格（跨分块），分块接缝处的实心邻接不产生边界。m_block_points
        // 留空：DeferLight2D 只在 m_block_mesh 缺失时才从 points 构建，
        // 此处直接持有预生成网格。
        // 轮廓段的逐像素排序 key 不是本层行号，而是该段自身的最低边，
        // 加上与本段格子严格相邻（8 邻域）的其它 BlockShadow 图层墙段：
        // 真正贴合的跨层上下段共享较低基座；隔开 2 格以上的墙不会把 key
        // 拉低。
        void write_part_blockshadow_mesh(const Tilemap::MapDocument& doc,
            const TilemapSystem::part_key& key, Light2D::BlockShadow* blockshadow)
        {
            const Tilemap::MapLayer& layer = doc.layers[key.layer];
            const int32_t bx = key.chunk_x * (int32_t)CHUNK;
            const int32_t by = key.chunk_y * (int32_t)CHUNK;
            const int32_t x0 = std::max(bx, 0);
            const int32_t x1 = std::min<int32_t>(bx + (int32_t)CHUNK, doc.width);
            const int32_t y0 = std::max(by, 0);
            const int32_t y1 = std::min<int32_t>(by + (int32_t)CHUNK, doc.height);

            // 跨层局部墙底：同一面墙的上下段可能位于不同的 tilemap 层。
            // 只合并与本段格子**严格相邻**（8 邻域，含同格）的其它
            // BlockShadow 层实心格，取局部最低底边作为 key：
            //  * 真正贴合的跨层墙段会共享较低基座，阴影连续；
            //  * 距离 2 格以上的另一段低墙不会被拉进来，避免上方墙的
            //    阴影被误判为前方而盖住带有 SelfShadow 的实体。
            const int32_t map_w = doc.width;
            const int32_t map_h = doc.height;
            const auto segment_key = [&](int32_t c0, int32_t c1,
                int32_t r0, int32_t r1, bool horizontal, float own_key) -> float
            {
                float result = own_key;

                const auto consider_cell = [&](int32_t x, int32_t y)
                {
                    if (x < 0 || y < 0 || x >= map_w || y >= map_h)
                        return;
                    for (const auto& candidate : doc.layers)
                    {
                        if (candidate.blockshadow == JE_LAYER_BLOCKSHADOW_OFF)
                            continue;
                        if (cell_blocks_light(doc, candidate, x, y))
                            result = std::min(result, -(float)(y + 1));
                    }
                };

                if (horizontal)
                {
                    for (int32_t cx = c0; cx < c1; ++cx)
                        for (int32_t dy = -1; dy <= 1; ++dy)
                            for (int32_t dx = -1; dx <= 1; ++dx)
                                consider_cell(cx + dx, r0 + dy);
                }
                else
                {
                    for (int32_t cy = r0; cy < r1; ++cy)
                        for (int32_t dy = -1; dy <= 1; ++dy)
                            for (int32_t dx = -1; dx <= 1; ++dx)
                                consider_cell(c0 + dx, cy + dy);
                }
                return result;
            };

            // (x, y, z=0, 挤出因子, 局部墙底 Y)
            // 因子 0 = 原位，1 = 背光挤出；局部墙底 Y 是该段及其邻近
            // 跨层墙段最靠前（Y 最小）的基座，用作整段的逐像素排序 key。
            std::vector<float> verts;
            auto push_point = [&verts](float px, float py, float f, float key_y)
            {
                verts.push_back(px);
                verts.push_back(py);
                verts.push_back(0.f);
                verts.push_back(f);
                verts.push_back(key_y);
            };
            // 一条边界线段（8 顶点）：A 的过渡点对（全原位，衔接上一段）
            // + A/B 实点对（原位/挤出，构成阴影四边形）+ B 的过渡点对
            //（全原位，衔接下一段）。段间过渡四边形的两侧点对挤出因子
            // 均为 0，两个三角形各有一对重合顶点 → 零面积不产生像素。
            auto push_segment = [&push_point](
                float ax, float ay, float ex, float ey, float key_y)
            {
                push_point(ax, ay, 0.f, key_y); push_point(ax, ay, 0.f, key_y);
                push_point(ax, ay, 0.f, key_y); push_point(ax, ay, 1.f, key_y);
                push_point(ex, ey, 0.f, key_y); push_point(ex, ey, 1.f, key_y);
                push_point(ex, ey, 0.f, key_y); push_point(ex, ey, 0.f, key_y);
            };

            // 四类边界各自独立成趟提取（上/下按行、左/右按列，各自贪心
            // 合并同条件 run）。必须独立成趟：若在同一趟里对同一 run 先后
            // 试探两类边界，1 格厚的 run 命中第一类后整段跳过，另一侧边界
            // 不会发射，薄墙在部分光源方向上就没有背光边。迎光侧边界由
            // 绕向配合 CULL(BACK) 剔除，四类全部发射对任意光源方向安全。
            // 方向约定：上边 -x、下边 +x（局部 y：上边 = -y，下边 =
            // -(y+1)）、左边 -y、右边 +y，实心保持在行进方向左侧
            for (int32_t y = y0; y < y1; ++y)
            {
                for (int32_t x = x0; x < x1;)
                {
                    if (cell_blocks_light(doc, layer, x, y)
                        && !cell_blocks_light(doc, layer, x, y - 1))
                    {
                        int32_t e = x + 1;
                        while (e < x1
                            && cell_blocks_light(doc, layer, e, y)
                            && !cell_blocks_light(doc, layer, e, y - 1))
                            ++e;
                        push_segment((float)e, -(float)y, (float)x, -(float)y,
                            segment_key(x, e, y, y, true, -(float)y));
                        x = e;
                        continue;
                    }
                    ++x;
                }
            }
            for (int32_t y = y0; y < y1; ++y)
            {
                for (int32_t x = x0; x < x1;)
                {
                    if (cell_blocks_light(doc, layer, x, y)
                        && !cell_blocks_light(doc, layer, x, y + 1))
                    {
                        int32_t e = x + 1;
                        while (e < x1
                            && cell_blocks_light(doc, layer, e, y)
                            && !cell_blocks_light(doc, layer, e, y + 1))
                            ++e;
                        push_segment((float)x, -(float)(y + 1),
                            (float)e, -(float)(y + 1),
                            segment_key(x, e, y, y, true, -(float)(y + 1)));
                        x = e;
                        continue;
                    }
                    ++x;
                }
            }
            for (int32_t x = x0; x < x1; ++x)
            {
                for (int32_t y = y0; y < y1;)
                {
                    if (cell_blocks_light(doc, layer, x, y)
                        && !cell_blocks_light(doc, layer, x - 1, y))
                    {
                        int32_t e = y + 1;
                        while (e < y1
                            && cell_blocks_light(doc, layer, x, e)
                            && !cell_blocks_light(doc, layer, x - 1, e))
                            ++e;
                        push_segment((float)x, -(float)y, (float)x, -(float)e,
                            segment_key(x, x + 1, y, e, false, -(float)e));
                        y = e;
                        continue;
                    }
                    ++y;
                }
            }
            for (int32_t x = x0; x < x1; ++x)
            {
                for (int32_t y = y0; y < y1;)
                {
                    if (cell_blocks_light(doc, layer, x, y)
                        && !cell_blocks_light(doc, layer, x + 1, y))
                    {
                        int32_t e = y + 1;
                        while (e < y1
                            && cell_blocks_light(doc, layer, x, e)
                            && !cell_blocks_light(doc, layer, x + 1, e))
                            ++e;
                        push_segment((float)(x + 1), -(float)e,
                            (float)(x + 1), -(float)y,
                            segment_key(x, x + 1, y, e, false, -(float)e));
                        y = e;
                        continue;
                    }
                    ++y;
                }
            }

            blockshadow->shape.m_block_points.clear();
            if (verts.empty())
            {
                // 分块无边界（全空/内部被邻接实心包围）：无阴影可投
                blockshadow->shape.m_block_mesh.reset();
                return;
            }

            std::vector<uint32_t> indices(verts.size() / 5);
            for (size_t i = 0; i < indices.size(); ++i)
                indices[i] = (uint32_t)i;

            auto created = graphic::vertex::create(
                JE_GL_VERTEX_TYPE_TRIANGLESTRIP,
                verts.data(), verts.size() * sizeof(float),
                indices,
                {
                    { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3 }, // pos
                    { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 1 }, // 挤出因子
                    { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 1 }, // 局部墙底 Y（逐像素排序 key）
                });
            if (created.has_value())
                blockshadow->shape.m_block_mesh = created;
        }

        // 图层遮挡阴影对齐（唯一的挂载点，新建分片次帧起由此挂载）：
        // OFF 移除分片上的 BlockShadow 组件，ON/AUTO 挂载并把 auto_disable
        // 对齐到图层模式、factor 对齐到图层强度。add_component 走命令
        // 缓冲、下一帧迁移进 arch 池，返回的是暂存实例——挂载分支写入
        // 暂存实例的遮挡轮廓随迁移转移进池；新组件的缺省 points 是单位
        // 方块，不清掉会被 DeferLight2D 当成轮廓。网格编辑经 rebuild_part
        // 随版本戳重建轮廓（读池内组件）
        void sync_part_blockshadow(const Tilemap::MapDocument* doc,
            const TilemapSystem::part_key& key, const game_entity& part,
            je_layer_block_shadow_t layer_blockshadow, float blockshadow_factor)
        {
            auto* blockshadow = part.get_component<Light2D::BlockShadow>();
            if (layer_blockshadow == JE_LAYER_BLOCKSHADOW_OFF)
            {
                if (blockshadow != nullptr)
                    part.remove_component<Light2D::BlockShadow>();
                return;
            }
            if (blockshadow == nullptr)
            {
                blockshadow = part.add_component<Light2D::BlockShadow>();
                if (doc != nullptr)
                    write_part_blockshadow_mesh(*doc, key, blockshadow);
            }
            blockshadow->factor = blockshadow_factor;
            blockshadow->reverse = false;
            blockshadow->auto_disable =
                layer_blockshadow == JE_LAYER_BLOCKSHADOW_AUTO;
            // 瓦片轮廓逐边生成：TopDown 下用顶点 key 让每个片元按所在边的
            // 世界 Y 参与逐像素深度排序，避免整个 chunk 共用 AABB 底边 key
            // 时覆盖站在其上的实体 SelfShadow；普通光源仍用实体 Z，与同层
            // SelfShadow 的 surface key 只差固定的半层偏移，由后者覆盖掉
            // 本层遮挡落在本层表面上的自遮挡。
            blockshadow->vertex_priority = true;
        }

        // 分块静态角点索引：每格 4 个四边形槽位，各 6 索引（0,1,2 2,1,3）
        const std::vector<uint32_t>& shared_indices()
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
        // uv 为 tile-local [0,1]²，layer 为数组纹理层号；
        // normal 为该瓦片的顶点法线（图集元数据，默认 (0,0,-1) 指向屏幕
        // 外侧）；bottom 为本块底边局部 Y，同块各顶点写同值）
        void write_quad(float* v,
            float x0, float y0, float x1, float y1,
            float u0, float v0, float u1, float v1, float layer, float bottom,
            const std::array<float, 3>& normal)
        {
            const float nx = normal[0], ny = normal[1], nz = normal[2];
            // 顶点序：0=左下 1=右下 2=左上 3=右上（索引 0,1,2 2,1,3）
            float* p = v;
            *p++ = x0; *p++ = y0; *p++ = 0.f; *p++ = u0; *p++ = v0; *p++ = layer; *p++ = nx; *p++ = ny; *p++ = nz; *p++ = bottom;
            *p++ = x1; *p++ = y0; *p++ = 0.f; *p++ = u1; *p++ = v0; *p++ = layer; *p++ = nx; *p++ = ny; *p++ = nz; *p++ = bottom;
            *p++ = x0; *p++ = y1; *p++ = 0.f; *p++ = u0; *p++ = v1; *p++ = layer; *p++ = nx; *p++ = ny; *p++ = nz; *p++ = bottom;
            *p++ = x1; *p++ = y1; *p++ = 0.f; *p++ = u1; *p++ = v1; *p++ = layer; *p++ = nx; *p++ = ny; *p++ = nz; *p++ = bottom;
        }

        // 把某格的解析结果写入 staging（16 顶点槽位：未用槽位写零面积
        // 退化四边形，不产生像素）。几何约定：1 格 = 1 世界单位
        //（tile_px 只用于图集采样 UV；根实体 LocalScale 即比例尺）。
        void write_cell(float* dst, int32_t x, int32_t y,
            const Tilemap::cell_quad_info* quads, int32_t count)
        {
            const float x0 = (float)x, x1 = x0 + 1.f;
            const float ytop = -(float)y, ybot = ytop - 1.f;
            const float half = 0.5f;
            const float xm = x0 + half, ym = ybot + half;

            for (size_t q = 0; q < 4; ++q)
                write_quad(dst + q * 4 * FLOATS_PER_VERTEX, x0, ytop, x0, ytop,
                    0.f, 0.f, 0.f, 0.f, 0.f, ybot, Tilemap::DEFAULT_TILE_NORMAL);
            for (int32_t i = 0; i < count; ++i)
            {
                const auto& q = quads[i];
                if (!q.corner.has_value())
                {
                    write_quad(dst, x0, ybot, x1, ytop,
                        q.u0, q.v0, q.u1, q.v1, (float)q.layer, ybot, q.normal);
                }
                else
                {
                    // 四象限槽位：0=LU 1=RU 2=LD 3=RD
                    float cx0, cy0, cx1, cy1;
                    switch (*q.corner)
                    {
                    case 0: cx0 = x0; cy0 = ym;  cx1 = xm; cy1 = ytop; break;
                    case 1: cx0 = xm;  cy0 = ym;  cx1 = x1; cy1 = ytop; break;
                    case 2: cx0 = x0; cy0 = ybot; cx1 = xm; cy1 = ym;  break;
                    default: cx0 = xm; cy0 = ybot; cx1 = x1; cy1 = ym;  break;
                    }
                    // 子象限仍写整格的 ybot：同格判定必须一致，避免自动图块混合格内缝
                    write_quad(dst + (size_t)*q.corner * 4 * FLOATS_PER_VERTEX,
                        cx0, cy0, cx1, cy1, q.u0, q.v0, q.u1, q.v1,
                        (float)q.layer, ybot, q.normal);
                }
            }
        }

        // ---- 网格扫描：收集某文档当前需要的全部分片 ----
        void scan_needed(Tilemap::MapDocument& doc, std::set<TilemapSystem::part_key>& needed)
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
                        const auto hit = ts->tile_cell(cell->local_id);
                        if (!hit.has_value())
                            continue;
                        const int32_t src_idx = std::get<0>(*hit);
                        needed.insert(TilemapSystem::part_key{
                            (int32_t)li, x / (int32_t)CHUNK, y / (int32_t)CHUNK,
                            cell->tileset_idx, src_idx });
                        // blob47 的显式变体可以来自同图集的其它源纹理：
                        // 这些源也要有分片承载对应格子（rebuild_part 按
                        // 解析出的源过滤，每个格子只会进入一个分片）
                        if (cell->is_auto
                            && ts->tile_kind(cell->local_id) == JE_TILE_KIND_BLOB47)
                        {
                            if (const auto* def = ts->tile_def(cell->local_id))
                                for (int32_t variant : def->variant_tiles)
                                {
                                    const auto vhit = ts->tile_cell(variant);
                                    if (vhit.has_value())
                                        needed.insert(TilemapSystem::part_key{
                                            (int32_t)li,
                                            x / (int32_t)CHUNK, y / (int32_t)CHUNK,
                                            cell->tileset_idx, std::get<0>(*vhit) });
                                }
                        }
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
        uint64_t scan_wall_mask(Tilemap::MapDocument& doc,
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
                        if (!ts->tile_walkable(cell->local_id))
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
        void merge_wall_rects(const std::vector<uint8_t>& blocked,
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
        void sync_wall_entity(const game_entity& e,
            const Tilemap::Wall& wall, const Transform::LocalScale* root_scale)
        {
            const float sx = root_scale != nullptr ? std::abs(root_scale->scale.x) : 1.f;
            const float sy = root_scale != nullptr ? std::abs(root_scale->scale.y) : 1.f;
            if (auto* rb = e.get_component<Physics2D::Rigidbody>())
                rb->layerid = wall.layerid;
            // 材质随动：物理系统按 Friction/Restitution 组件值重建 fixture，
            // 这里对齐组件值即可让 Wall 侧的运行时修改即时生效
            if (auto* fric = e.get_component<Physics2D::Friction>())
                fric->value = wall.friction;
            if (auto* rest = e.get_component<Physics2D::Restitution>())
                rest->value = wall.restitution;
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
    }

    bool TilemapSystem::part_key::operator<(const TilemapSystem::part_key& o) const
    {
        return std::tie(layer, chunk_x, chunk_y, tileset, source)
            < std::tie(o.layer, o.chunk_x, o.chunk_y, o.tileset, o.source);
    }

    TilemapSystem::TilemapSystem(game_world w)
        : game_system(w)
    {
        _m_staging.assign(CHUNK * CHUNK * FLOATS_PER_CELL, 0.f);
    }

    // ---- 分片生命周期 ----

    game_entity TilemapSystem::create_part_entity(game_entity root, int32_t layer, float layer_z,
        int32_t layer_rend_queue, je_layer_self_shadow_t layer_selfshadow,
        float selfshadow_factor,
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

        // 图层自阴影（Light2D 管线）：OFF 不挂组件；ON/AUTO 挂载且
        // auto_disable 分别固定 false/true，factor 取图层强度。
        // 网格顶点带块底边属性，同挂 PerBlockSelfShadow 供 TopDown
        // 光源逐格判定
        if (layer_selfshadow != JE_LAYER_SELFSHADOW_OFF)
        {
            auto* selfshadow = part.add_component<Light2D::SelfShadow>();
            selfshadow->factor = selfshadow_factor;
            selfshadow->auto_disable =
                layer_selfshadow == JE_LAYER_SELFSHADOW_AUTO;
            part.add_component<Light2D::PerBlockSelfShadow>();
        }

        // 图层遮挡阴影（Light2D 管线）不在此挂载：add_component 走
        // 命令缓冲、下一帧才迁移进 arch 池，本帧随后 rebuild_part 的
        // get_component 读不到（轮廓 mesh 会漏写）。挂载与轮廓生成
        // 统一由下一帧起的 sync_part_blockshadow 完成

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
        auto* arrays = part.get_component<Renderer::Textures2DArray>();
        if (src.array.has_value())
            arrays->bind_texture(0, *src.array);
        else
            debug::logerr(
                "Tilemap: source '%s' has no texture array built yet.",
                src.path.c_str());
        // 附加层级（如法线图集）按各自 pass 绑入同一数组组件；
        // 未就绪的层级留空（refresh_part_arrays 逐帧兜底对齐）
        for (const auto& e : src.extras)
            if (e.array.has_value())
                arrays->bind_texture((size_t)e.pass, *e.array);

        // 图层样式的附加 2D 纹理占其余槽（槽 >= 1）
        auto* textures = part.get_component<Renderer::Textures2D>();
        for (const auto& t : style.textures)
            textures->bind_texture((size_t)t.slot, t.texture);

        auto* shape = part.get_component<Renderer::Shape>();
        auto created = graphic::vertex::create(
            JE_GL_VERTEX_TYPE_TRIANGLES,
            _m_staging.data(),
            _m_staging.size() * sizeof(float),
            shared_indices(),
            {
                { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3 }, // pos
                { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3 }, // uv (u, v, array layer)
                { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3 }, // normal
                { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 1 }, // bottom（块底边局部 Y，PerBlockSelfShadow 用）
            });
        if (created.has_value())
            shape->vertex = created;

        return part;
    }

    void TilemapSystem::rebuild_part(Tilemap::MapDocument& doc, game_entity root,
        const TilemapSystem::part_key& key, game_entity e)
    {
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

        je_gl_vertex_t* raw = shape->vertex->get()->resource();
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

        // 遮挡轮廓跟随网格内容：分片挂有 BlockShadow 时随版本戳
        //（网格编辑/图层模式切换）重建轮廓条带
        if (auto* blockshadow = e.get_component<Light2D::BlockShadow>())
            write_part_blockshadow_mesh(doc, key, blockshadow);
    }

    // ---- 碰撞体墙 ----

    game_entity TilemapSystem::create_wall_entity(game_entity root, const Tilemap::Wall& wall,
        const Transform::LocalScale* root_scale,
        int32_t x0, int32_t y0, int32_t x1, int32_t y1)
    {
        auto world = get_world();
        // 静态碰撞体：无 DynamicBody/KinematicBody 即 Static（墙不可推动）
        auto e = world.add_entity<
            Transform::LocalPosition, Transform::LocalRotation, Transform::LocalScale,
            Transform::LocalToParent, Transform::Translation,
            Physics2D::Rigidbody, Physics2D::Collider::Box,
            Physics2D::Friction, Physics2D::Restitution,
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

    void TilemapSystem::rebuild_walls(game_entity root,
        const Tilemap::Wall& wall,
        const std::vector<je_game_entity_t>& stale_walls,
        const std::vector<uint8_t>& blocked,
        int32_t mask_width, int32_t mask_height, bool block_empty)
    {
        auto world = get_world();
        for (const auto& raw : stale_walls)
            world.remove_entity(game_entity{ raw });

        const auto* root_scale = root.get_component<Transform::LocalScale>();
        std::vector<int32_t> rects;
        merge_wall_rects(blocked, mask_width, mask_height, rects);
        const int32_t off = block_empty ? 1 : 0;
        for (size_t r = 0; r + 3 < rects.size(); r += 4)
            create_wall_entity(
                root, wall, root_scale,
                rects[r] - off, rects[r + 1] - off,
                rects[r + 2] - off, rects[r + 3] - off);
    }

    void TilemapSystem::sync_root_walls(game_entity root, TilemapSystem::root_state& state,
        Tilemap::Wall* wall_comp, Tilemap::MapDocument* doc, uint64_t stamp,
        const std::vector<je_game_entity_t>& live_walls)
    {
        if (wall_comp == nullptr || doc == nullptr)
        {
            if (!live_walls.empty())
            {
                auto world = get_world();
                for (const auto& raw : live_walls)
                    world.remove_entity(game_entity{ raw });
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
                rebuild_walls(root, *wall_comp, live_walls,
                    _m_wall_mask, mw, mh, wall_comp->block_empty);
                state.wall_mask = mask_hash;
                state.wall_block_empty = wall_comp->block_empty;
            }
            state.wall_stamp = stamp;
            state.wall_valid = true;
        }
        const auto* root_scale = root.get_component<Transform::LocalScale>();
        for (const auto& raw : live_walls)
            sync_wall_entity(game_entity{ raw }, *wall_comp, root_scale);
    }

    void TilemapSystem::StateUpdate()
    {
        auto world = get_world();
        auto& docs = Tilemap::documents::inst();

        // 1. 收集地图根实体并解析文档（Wall 为可选的碰撞体开关）
        struct root_info
        {
            je_game_entity_t raw{};
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

        // 2. 回收孤儿分片/墙体，并收割现役实体句柄。实体句柄禁止
        //    跨帧缓存：碎片整理与组件增删（如自阴影/遮挡阴影挂载）
        //    都会移动实体，隔帧句柄的 get_component 会静默返回空，
        //    分片重建/对齐被悄悄跳过（表现为绘制不同步）——分片与
        //    墙体一律以当帧查询结果定位，簿记只存纯数据
        std::vector<je_game_entity_t> dead_parts;
        std::map<std::tuple<uint32_t, int32_t, int32_t, int32_t, int32_t>,
            je_game_entity_t> live_parts;
        std::map<uint32_t, std::vector<je_game_entity_t>> live_walls;
        for (auto&& [e, part] :
            query_entity<view typesof(Tilemap::RenderPart&)>())
        {
            if (alive_root_ids.count(part.owner_id) == 0)
            {
                dead_parts.push_back(e._m_raw);
                continue;
            }
            live_parts[
            {
                part.owner_id, part.layer,
                part.chunk_x, part.chunk_y, part.source
            }] = e._m_raw;
        }
        for (auto&& [e, part] :
            query_entity<view typesof(Tilemap::WallPart&)>())
        {
            if (alive_root_ids.count(part.owner_id) == 0)
                dead_parts.push_back(e._m_raw);
            else
                live_walls[part.owner_id].push_back(e._m_raw);
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

            // 删除不再需要的分片（实体以当帧句柄回收；簿记里已无
            // 实体的键一并清掉，走下方重建路径）
            std::vector<part_key> removed;
            for (auto& [key, stamp_built] : state.parts)
                if (needed.count(key) == 0)
                    removed.push_back(key);
            for (const auto& key : removed)
            {
                auto live = live_parts.find(
                {
                    root_id, key.layer, key.chunk_x, key.chunk_y,
                    key.tileset * 1024 + key.source
                });
                if (live != live_parts.end())
                    world.remove_entity(game_entity{ live->second });
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
                const je_layer_self_shadow_t layer_selfshadow =
                    doc != nullptr
                    && key.layer < (int32_t)doc->layers.size()
                    ? doc->layers[key.layer].selfshadow : JE_LAYER_SELFSHADOW_OFF;
                const float layer_selfshadow_factor =
                    doc != nullptr
                    && key.layer < (int32_t)doc->layers.size()
                    ? doc->layers[key.layer].selfshadow_factor : 1.f;
                const je_layer_block_shadow_t layer_blockshadow =
                    doc != nullptr
                    && key.layer < (int32_t)doc->layers.size()
                    ? doc->layers[key.layer].blockshadow : JE_LAYER_BLOCKSHADOW_OFF;
                const float layer_blockshadow_factor =
                    doc != nullptr
                    && key.layer < (int32_t)doc->layers.size()
                    ? doc->layers[key.layer].blockshadow_factor : 1.f;

                // 源纹理（主数组与附加层级数组的唯一来源）：文档与
                // 层样式都就绪时解析，供已有分片对齐与新建分片共用
                Tilemap::SourceTexture* src_info = nullptr;
                if (doc != nullptr
                    && key.tileset < (int32_t)doc->tilesets.size()
                    && doc->tilesets[key.tileset].resolved.has_value()
                    && key.source < (int32_t)doc->tilesets[key.tileset].resolved->get()->sources.size())
                    src_info = &doc->tilesets[key.tileset].resolved->get()->sources[key.source];

                // 数组纹理就绪兜底（主数组与各附加层级）：未就绪则
                // 本次留空（分片该通道缺绑），后续帧重试；图集重载后
                // 数组随新实例重建亦走此路径。构建失败置 array_failed，
                // 只试一次（纹理加载失败会逐次告警，避免逐帧刷日志）
                if (src_info != nullptr)
                {
                    if (!src_info->array.has_value() && !src_info->array_failed)
                        if (!Tilemap::build_source_array(*src_info))
                            src_info->array_failed = true;
                    for (auto& ex : src_info->extras)
                        if (!ex.array.has_value() && !ex.array_failed)
                            if (!Tilemap::build_extra_array(*src_info, ex))
                                ex.array_failed = true;
                }

                // 当帧分片句柄：与簿记配对（簿记有而实体无 = 被外部
                // 销毁，丢弃簿记走重建；实体有而簿记无 = 异常路径，
                // 收编并强制重建一次；两者皆无 = 首建）
                auto live = live_parts.find(
                {
                    root_id, key.layer, key.chunk_x, key.chunk_y,
                    key.tileset * 1024 + key.source
                });
                auto fnd = state.parts.find(key);
                if (live == live_parts.end() && fnd != state.parts.end())
                {
                    state.parts.erase(fnd);
                    fnd = state.parts.end();
                }
                if (live != live_parts.end() && fnd == state.parts.end())
                    fnd = state.parts.emplace(key, 0).first;
                if (fnd != state.parts.end())
                {
                    game_entity part{ live->second };
                    if (doc != nullptr && fnd->second != stamp)
                    {
                        rebuild_part(*doc, root, key, part);
                        fnd->second = stamp;
                    }
                    // 着色器/2D 附加纹理跟随图层样式，数组纹理跟随
                    // 图集源纹理（主数组 + 附加层级），比例尺跟随根
                    // 实体，自阴影与遮挡阴影组件跟随图层模式（组件与
                    // 样式/图集变化均不计入地图版本戳，故每帧对齐）
                    refresh_part_shader(style, part);
                    refresh_part_textures(part, style);
                    refresh_part_arrays(part, src_info);
                    sync_part_scale(root, part);
                    sync_part_selfshadow(part, layer_selfshadow,
                        layer_selfshadow_factor);
                    sync_part_blockshadow(doc, key, part, layer_blockshadow,
                        layer_blockshadow_factor);
                    continue;
                }

                if (src_info == nullptr)
                    continue;

                game_entity part_entity = create_part_entity(
                    root, key.layer, doc->layers[key.layer].z,
                    doc->layers[key.layer].rend_queue,
                    doc->layers[key.layer].selfshadow,
                    doc->layers[key.layer].selfshadow_factor,
                    *src_info, style != nullptr ? *style : Tilemap::LayerStyle{});
                auto* tag = part_entity.get_component<Tilemap::RenderPart>();
                tag->owner_id = root_id;
                tag->layer = key.layer;
                tag->chunk_x = key.chunk_x;
                tag->chunk_y = key.chunk_y;
                tag->source = key.tileset * 1024 + key.source;

                // 新实体句柄仅本帧内使用（创建是直写 arch，本帧即可
                // 读写组件；落簿记的只有版本戳）
                uint64_t built_stamp = 0;
                if (doc != nullptr)
                {
                    rebuild_part(*doc, root, key, part_entity);
                    built_stamp = stamp;
                }
                state.parts[key] = built_stamp;
            }

            // 4. 碰撞体墙同步：Wall 组件在位时按不可通行掩码
            //    生成/回收静态 Physics2D 碰撞体子实体（句柄取当帧
            //    WallPart 查询结果）
            {
                auto fnd_walls = live_walls.find(root_id);
                sync_root_walls(root, state, r.wall, doc, stamp,
                    fnd_walls != live_walls.end()
                    ? fnd_walls->second : std::vector<je_game_entity_t>{});
            }
        }
    }
}

// ======================================================================
// je_tilemap_* C API 实现（声明与 JE_API 导出标记统一见 include/jeecs.hpp）
// ======================================================================

// ---------------- 地图文档 ----------------
/* OPTIONAL: If path cannot be opened */ je_tilemap_document_t* je_tilemap_map_open(const char* path)
{
    assert(path != nullptr);
    // open_map 返回自带 1 个外部引用的实例（unload/gchandle 回收时
    // 释放）；打开失败（文件缺失/损坏，含已标记失败不重试）为 nullptr
    auto doc = jeecs::Tilemap::documents::inst().open_map(path);
    return doc.has_value() ? doc->get() : nullptr;
}
/* OPTIONAL: If size is invalid */ je_tilemap_document_t* je_tilemap_map_create(int32_t w, int32_t h, int32_t tile_px)
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
int32_t je_tilemap_map_keep(je_tilemap_document_t* map)
{
    return jeecs::Tilemap::documents::inst().keep_map(map);
}
int32_t je_tilemap_map_unload(je_tilemap_document_t* map)
{
    return jeecs::Tilemap::documents::inst().unload_map(map);
}
bool je_tilemap_map_path_bind(je_tilemap_document_t* map, const char* path)
{
    assert(path != nullptr);
    return jeecs::Tilemap::documents::inst().bind_map_path(map, path);
}
void je_tilemap_map_reload(const char* path)
{
    assert(path != nullptr);
    jeecs::Tilemap::documents::inst().reload_map(path);
}
void je_tilemap_map_size(je_tilemap_document_t* doc, /* OPTIONAL */ int32_t* w, /* OPTIONAL */ int32_t* h, /* OPTIONAL */ int32_t* tile_px, /* OPTIONAL */ int32_t* layer_count)
{
    if (w != nullptr) *w = doc->width;
    if (h != nullptr) *h = doc->height;
    if (tile_px != nullptr) *tile_px = doc->tile_px;
    if (layer_count != nullptr) *layer_count = (int32_t)doc->layers.size();
}
bool je_tilemap_map_path(je_tilemap_document_t* doc, const char** out_path)
{
    assert(out_path != nullptr);
    *out_path = doc->path.c_str();
    return !doc->path.empty();
}
uint64_t je_tilemap_map_version(je_tilemap_document_t* doc)
{
    return doc->version;
}
int32_t je_tilemap_loaded_map_count(void)
{
    return jeecs::Tilemap::documents::inst().map_count();
}
/* OPTIONAL: If index is out of range or the slot is empty */ je_tilemap_document_t* je_tilemap_loaded_map(int32_t index)
{
    // 调用期间由 map_at 锁内的持有保活，用毕即弃、不占外部引用
    auto doc = jeecs::Tilemap::documents::inst().map_at(index);
    return doc.has_value() ? doc->get() : nullptr;
}
void je_tilemap_map_resize(je_tilemap_document_t* doc, int32_t new_w, int32_t new_h)
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
bool je_tilemap_layer_add(je_tilemap_document_t* doc, const char* name, je_layer_id_t* out_id)
{
    assert(name != nullptr);
    assert(out_id != nullptr);
    jeecs::Tilemap::MapLayer layer;
    layer.name = name;
    layer.z = -(float)doc->layers.size(); // 新层默认置于最上（沿 -z 错开）
    layer.rend_queue = jeecs::Tilemap::LAYER_REND_QUEUE_BASE
        + (int32_t)doc->layers.size();   // 渲染队列同样按下标错开（升序绘制）
    layer.grid.assign((size_t)doc->width * doc->height, 0);
    doc->layers.push_back(std::move(layer));
    ++doc->version;
    *out_id = (int32_t)doc->layers.size() - 1;
    return true;
}
bool je_tilemap_layer_remove(je_tilemap_document_t* doc, je_layer_id_t layer)
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
bool je_tilemap_layer_move(je_tilemap_document_t* doc, je_layer_id_t layer, je_layer_id_t new_pos)
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
void je_tilemap_layer_get(je_tilemap_document_t* doc, je_layer_id_t layer,
    /* OPTIONAL */ const char** name, /* OPTIONAL */ int32_t* visible, /* OPTIONAL */ int32_t* locked)
{
    const jeecs::Tilemap::MapLayer* l =
        layer >= 0 && layer < (int32_t)doc->layers.size()
        ? &doc->layers[layer] : nullptr;
    if (name != nullptr) *name = l ? l->name.c_str() : "";
    if (visible != nullptr) *visible = l ? (l->visible ? 1 : 0) : 0;
    if (locked != nullptr) *locked = l ? (l->locked ? 1 : 0) : 0;
}
void je_tilemap_layer_set(je_tilemap_document_t* doc, je_layer_id_t layer,
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
float je_tilemap_layer_z(je_tilemap_document_t* doc, je_layer_id_t layer)
{
    return layer >= 0 && layer < (int32_t)doc->layers.size()
        ? doc->layers[layer].z : 0.f;
}
void je_tilemap_layer_z_set(je_tilemap_document_t* doc, je_layer_id_t layer, float z)
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
int32_t je_tilemap_layer_rend_queue(je_tilemap_document_t* doc, je_layer_id_t layer)
{
    return layer >= 0 && layer < (int32_t)doc->layers.size()
        ? doc->layers[layer].rend_queue : 0;
}
void je_tilemap_layer_rend_queue_set(je_tilemap_document_t* doc, je_layer_id_t layer, int32_t rend_queue)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size())
        return;
    if (doc->layers[layer].rend_queue != rend_queue)
    {
        doc->layers[layer].rend_queue = rend_queue;
        ++doc->version;
    }
}

// 图层的 2D 光照自阴影行为：OFF/AUTO/ON 分别对应分片不挂
// Light2D::SelfShadow、挂载且 auto_disable=true、挂载且 auto_disable=false
je_layer_self_shadow_t je_tilemap_layer_selfshadow(je_tilemap_document_t* doc, je_layer_id_t layer)
{
    return layer >= 0 && layer < (int32_t)doc->layers.size()
        ? doc->layers[layer].selfshadow : JE_LAYER_SELFSHADOW_OFF;
}
void je_tilemap_layer_selfshadow_set(je_tilemap_document_t* doc, je_layer_id_t layer,
    je_layer_self_shadow_t selfshadow)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size()
        || selfshadow < JE_LAYER_SELFSHADOW_OFF
        || selfshadow > JE_LAYER_SELFSHADOW_AUTO)
        return;
    if (doc->layers[layer].selfshadow != selfshadow)
    {
        doc->layers[layer].selfshadow = selfshadow;
        ++doc->version;
    }
}

// 图层的自阴影覆盖强度（写入分片 SelfShadow::factor）：仅图层模式为
// ON/AUTO 时生效；缺省 1，不做范围钳制（与组件本体的 factor 同口径）
float je_tilemap_layer_selfshadow_factor(je_tilemap_document_t* doc, je_layer_id_t layer)
{
    return layer >= 0 && layer < (int32_t)doc->layers.size()
        ? doc->layers[layer].selfshadow_factor : 1.f;
}
void je_tilemap_layer_selfshadow_factor_set(je_tilemap_document_t* doc,
    je_layer_id_t layer, float factor)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size())
        return;
    if (doc->layers[layer].selfshadow_factor != factor)
    {
        doc->layers[layer].selfshadow_factor = factor;
        ++doc->version;
    }
}

// 图层的遮挡阴影行为：OFF/AUTO/ON 分别对应分片不挂
// Light2D::BlockShadow、挂载且 auto_disable=true、挂载且 auto_disable=false
je_layer_block_shadow_t je_tilemap_layer_blockshadow(je_tilemap_document_t* doc, je_layer_id_t layer)
{
    return layer >= 0 && layer < (int32_t)doc->layers.size()
        ? doc->layers[layer].blockshadow : JE_LAYER_BLOCKSHADOW_OFF;
}
void je_tilemap_layer_blockshadow_set(je_tilemap_document_t* doc, je_layer_id_t layer,
    je_layer_block_shadow_t blockshadow)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size()
        || blockshadow < JE_LAYER_BLOCKSHADOW_OFF
        || blockshadow > JE_LAYER_BLOCKSHADOW_AUTO)
        return;
    if (doc->layers[layer].blockshadow != blockshadow)
    {
        doc->layers[layer].blockshadow = blockshadow;
        ++doc->version;
    }
}

// 图层的遮挡阴影浓度（写入分片 BlockShadow::factor）：仅图层模式为
// ON/AUTO 时生效；缺省 1，不做范围钳制（与组件本体的 factor 同口径）
float je_tilemap_layer_blockshadow_factor(je_tilemap_document_t* doc, je_layer_id_t layer)
{
    return layer >= 0 && layer < (int32_t)doc->layers.size()
        ? doc->layers[layer].blockshadow_factor : 1.f;
}
void je_tilemap_layer_blockshadow_factor_set(je_tilemap_document_t* doc,
    je_layer_id_t layer, float factor)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size())
        return;
    if (doc->layers[layer].blockshadow_factor != factor)
    {
        doc->layers[layer].blockshadow_factor = factor;
        ++doc->version;
    }
}

// ---------------- 图层着色器样式 ----------------
// 样式修改只 bump 层内 style_version（不 bump doc->version）：样式与
// 网格内容正交，不应触发渲染侧的全网格重扫与顶点重建。
// 不透明指针 je_tilemap_shader_ref_t / je_tilemap_texture_ref_t 在此即
// basic::resource<graphic::shader/texture>*（见 include/jeecs.hpp）。
/* OPTIONAL: If layer has no shader style */ je_tilemap_shader_ref_t* je_tilemap_layer_shader(je_tilemap_document_t* doc, je_layer_id_t layer)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size()
        || !doc->layers[layer].style.shader.has_value())
        return nullptr;
    return (je_tilemap_shader_ref_t*)new jeecs::basic::resource<jeecs::graphic::shader>(
        doc->layers[layer].style.shader.value());
}
void je_tilemap_layer_shader_set(je_tilemap_document_t* doc, je_layer_id_t layer,
    /* OPTIONAL */ je_tilemap_shader_ref_t* shader)
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
int32_t je_tilemap_layer_texture_count(je_tilemap_document_t* doc, je_layer_id_t layer)
{
    return layer >= 0 && layer < (int32_t)doc->layers.size()
        ? (int32_t)doc->layers[layer].style.textures.size() : 0;
}
bool je_tilemap_layer_texture_set(je_tilemap_document_t* doc, je_layer_id_t layer,
    int32_t slot, /* OPTIONAL */ je_tilemap_texture_ref_t* texture)
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
bool je_tilemap_layer_texture_get(je_tilemap_document_t* doc, je_layer_id_t layer,
    int32_t index, /* OPTIONAL */ int32_t* slot, /* OPTIONAL */ je_tilemap_texture_ref_t** texture)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size()
        || index < 0 || index >= (int32_t)doc->layers[layer].style.textures.size())
        return false;
    const auto& t = doc->layers[layer].style.textures[index];
    if (slot != nullptr) *slot = t.slot;
    if (texture != nullptr)
        *texture = (je_tilemap_texture_ref_t*)
        new jeecs::basic::resource<jeecs::graphic::texture2d>(t.texture);
    return true;
}
void je_tilemap_shader_ref_free(/* OPTIONAL */ je_tilemap_shader_ref_t* shader)
{
    delete (jeecs::basic::resource<jeecs::graphic::shader>*)shader;
}
void je_tilemap_texture_ref_free(/* OPTIONAL */ je_tilemap_texture_ref_t* texture)
{
    delete (jeecs::basic::resource<jeecs::graphic::texture2d>*)texture;
}

// ---------------- 瓦片读写 ----------------
int32_t je_tilemap_tile_get(je_tilemap_document_t* doc, je_layer_id_t layer, int32_t x, int32_t y)
{
    if (!doc->in_bounds(layer, x, y))
        return 0;
    return doc->layers[layer].grid[y * doc->width + x];
}
/* OPTIONAL: If layer is invalid */ const int32_t* je_tilemap_layer_grid(
    je_tilemap_document_t* doc, je_layer_id_t layer, /* OPTIONAL */ int32_t* out_count)
{
    if (layer < 0 || layer >= (int32_t)doc->layers.size())
        return nullptr;
    if (out_count != nullptr)
        *out_count = (int32_t)doc->layers[layer].grid.size();
    return doc->layers[layer].grid.data();
}
void je_tilemap_tile_set(je_tilemap_document_t* doc, je_layer_id_t layer, int32_t x, int32_t y, int32_t value)
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
void je_tilemap_tile_fill(je_tilemap_document_t* doc, je_layer_id_t layer,
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
int32_t je_tilemap_tileset_count(je_tilemap_document_t* doc)
{
    return (int32_t)doc->tilesets.size();
}
bool je_tilemap_tileset_add(je_tilemap_document_t* doc, const char* path, int32_t* out_index)
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
bool je_tilemap_tileset_remove(je_tilemap_document_t* doc, int32_t idx)
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
                v = cell->is_auto
                    ? jeecs::Tilemap::encode_auto_value(moved, cell->local_id)
                    : jeecs::Tilemap::encode_tile_value(moved, cell->local_id);
            }
        }
    ++doc->version;
    return true;
}
const char* je_tilemap_tileset_path(je_tilemap_document_t* doc, int32_t idx)
{
    if (idx < 0 || idx >= (int32_t)doc->tilesets.size())
        return "";
    return doc->tilesets[idx].path.c_str();
}

// ---------------- 属性与查询 ----------------
// void* <-> property_value 的按类型编组：je_property_type_t 与 variant 备选
// 下标一一对应，类型严格匹配（不匹配即失败，不做隐式转换）。数据形态
// 见 jeecs.hpp 中 je_property_type_t 的说明。
static bool je_tilemap_write_property(
    jeecs::Tilemap::property_value& out, je_property_type_t type, const void* value)
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
    const jeecs::Tilemap::property_value& v, je_property_type_t type, void* out_value)
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

bool je_tilemap_cell_property_set(je_tilemap_document_t* doc, je_layer_id_t layer,
    int32_t x, int32_t y, const char* name, je_property_type_t type,
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
bool je_tilemap_cell_property_get(je_tilemap_document_t* doc, je_layer_id_t layer,
    int32_t x, int32_t y, const char* name, je_property_type_t type,
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
bool je_tilemap_property_get(je_tilemap_document_t* doc, je_layer_id_t layer,
    int32_t x, int32_t y, const char* name, je_property_type_t type,
    /* OPTIONAL */ void* out_value)
{
    assert(name != nullptr);
    assert(out_value != nullptr);
    const jeecs::Tilemap::property_value* r =
        jeecs::Tilemap::resolve_property(doc, layer, x, y, name);
    return r == nullptr ? false : je_tilemap_read_property(*r, type, out_value);
}
int32_t je_tilemap_cell_property_entries(je_tilemap_document_t* doc,
    /* OPTIONAL */ je_tilemap_cell_property_entry_t* out_entries, int32_t capacity)
{
    int32_t written = 0;
    for (const auto& [key, values] : doc->cell_properties)
    {
        if (out_entries != nullptr && written < capacity)
        {
            auto& entry = out_entries[written];
            entry.m_layer = key.first;
            entry.m_y = key.second / doc->width;
            entry.m_x = key.second % doc->width;
            entry.m_property_count = (int32_t)values.size();
        }
        ++written;
    }
    return written;
}
bool je_tilemap_cell_property_entry_property_info(je_tilemap_document_t* doc,
    je_layer_id_t layer, int32_t x, int32_t y, int32_t index,
    /* OPTIONAL */ const char** name, /* OPTIONAL */ je_property_type_t* type)
{
    if (!doc->in_bounds(layer, x, y))
        return false;
    auto fnd = doc->cell_properties.find({ layer, x + y * doc->width });
    if (fnd == doc->cell_properties.end())
        return false;
    if (index < 0 || index >= (int32_t)fnd->second.size())
        return false;

    auto it = fnd->second.begin();
    std::advance(it, index);
    if (name != nullptr)
        *name = it->first.c_str();
    if (type != nullptr)
        *type = (je_property_type_t)it->second.index();
    return true;
}
int32_t je_tilemap_is_walkable(je_tilemap_document_t* doc, int32_t x, int32_t y)
{
    if (!doc->in_bounds(0, x, y))
        return 0;   // 出界视为不可通行
    for (int32_t l = 0; l < (int32_t)doc->layers.size(); ++l)
        if (!jeecs::Tilemap::walkable_on(doc, l, x, y))
            return 0;
    return 1;
}
int32_t je_tilemap_is_walkable_on(je_tilemap_document_t* doc, je_layer_id_t layer, int32_t x, int32_t y)
{
    return jeecs::Tilemap::walkable_on(doc, layer, x, y) ? 1 : 0;
}
int32_t je_tilemap_cell_find(je_tilemap_document_t* doc, je_layer_id_t layer,
    const char* name, je_property_type_t type, const void* value)
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
bool je_tilemap_cell_find_get(je_tilemap_document_t* doc, int32_t index, int32_t* x, int32_t* y)
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
/* OPTIONAL: If path cannot be opened */ je_tileset_document_t* je_tilemap_tileset_open(const char* path)
{
    assert(path != nullptr);
    // open_tileset 返回自带 1 个外部引用的实例（unload/gchandle 回收时
    // 释放）；打开失败（文件缺失/损坏，含已标记失败不重试）为 nullptr
    auto doc = jeecs::Tilemap::documents::inst().open_tileset(path);
    return doc.has_value() ? doc->get() : nullptr;
}
je_tileset_document_t* je_tilemap_tileset_create(const char* name)
{
    assert(name != nullptr);
    auto doc = std::make_shared<jeecs::Tilemap::TilesetDocument>();
    doc->name = name;
    // register_tileset 返回自带 1 个引用的实例
    return jeecs::Tilemap::documents::inst().register_tileset(std::move(doc));
}
int32_t je_tilemap_tileset_keep(je_tileset_document_t* tileset)
{
    return jeecs::Tilemap::documents::inst().keep_tileset(tileset);
}
int32_t je_tilemap_tileset_unload(je_tileset_document_t* tileset)
{
    return jeecs::Tilemap::documents::inst().unload_tileset(tileset);
}
bool je_tilemap_tileset_path_bind(je_tileset_document_t* tileset, const char* path)
{
    assert(path != nullptr);
    return jeecs::Tilemap::documents::inst().bind_tileset_path(tileset, path);
}
void je_tilemap_tileset_reload(const char* path)
{
    assert(path != nullptr);
    jeecs::Tilemap::documents::inst().reload_tileset(path);
}
uint64_t je_tilemap_tileset_version(je_tileset_document_t* doc)
{
    return doc->version;
}
const char* je_tilemap_tileset_doc_path(je_tileset_document_t* doc)
{
    return doc->path.c_str();
}
void je_tilemap_tileset_name_set(je_tileset_document_t* doc, const char* name)
{
    assert(name != nullptr);
    doc->name = name;
    ++doc->version;
}
const char* je_tilemap_tileset_name(je_tileset_document_t* doc)
{
    return doc->name.c_str();
}
int32_t je_tilemap_tileset_next_tile_id(je_tileset_document_t* doc)
{
    return doc->next_tile_id;
}

// ---------------- 图集源纹理 ----------------
bool je_tilemap_source_add(je_tileset_document_t* doc, const char* texture_path,
    int32_t tile_px, je_source_id_t* out_id)
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
            *out_id = (je_source_id_t)i;
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
    // 为源的每个格子分配稳定瓦片 id（行优先连续分配）。id 只增不减：
    // 删除源/纹理裁掉格子后其 id 作废永不复用，旧地图按 id 引用不会
    // 漂移到别的格子。瓦片 id 须能编入 32 位瓦片值的低位。
    const int64_t cell_count = (int64_t)src.xcount * src.ycount;
    if ((int64_t)doc->next_tile_id + cell_count
        > (int64_t)jeecs::Tilemap::TILE_ID_BASE)
    {
        jeecs::debug::logerr(
            "Tilemap: too many tiles in tileset (limit %d).",
            jeecs::Tilemap::TILE_ID_BASE);
        return false;
    }
    src.cell_ids.resize((size_t)cell_count);
    for (int64_t k = 0; k < cell_count; ++k)
        src.cell_ids[(size_t)k] = doc->next_tile_id++;
    // 展开为"每层一个瓦片"的数组纹理（分片采样用）
    if (!jeecs::Tilemap::build_source_array(src))
        jeecs::debug::logwarn(
            "Tilemap: texture array for '%s' deferred (image not ready).",
            texture_path);
    doc->sources.push_back(std::move(src));
    doc->rebuild_tile_cells();
    ++doc->version;
    *out_id = (je_source_id_t)doc->sources.size() - 1;
    return true;
}
bool je_tilemap_source_remove(je_tileset_document_t* doc, je_source_id_t source_idx)
{
    if (source_idx < 0 || source_idx >= (int32_t)doc->sources.size())
        return false;
    // 先丢弃该源各格的瓦片元数据（id 作废），再移除源；其余源的格 id
    // 不随结构体移动/下标重排改变。
    for (int32_t tile_id : doc->sources[source_idx].cell_ids)
        doc->tiles.erase(tile_id);
    doc->sources.erase(doc->sources.begin() + source_idx);
    doc->rebuild_tile_cells();
    // 引用被删格子的 blob47 变体一并剔除（与 reconcile_source_grid 的
    // 裁剪口径一致），否则保存出的文件会包含悬空 id、再次加载被拒绝。
    for (auto& entry : doc->tiles)
    {
        auto& variants = entry.second.variant_tiles;
        variants.erase(std::remove_if(variants.begin(), variants.end(),
            [&](int32_t vid) { return !doc->tile_cell(vid).has_value(); }),
            variants.end());
    }
    ++doc->version;
    return true;
}
int32_t je_tilemap_source_count(je_tileset_document_t* doc)
{
    return (int32_t)doc->sources.size();
}
bool je_tilemap_source_info(je_tileset_document_t* doc, je_source_id_t source_idx,
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
bool je_tilemap_source_tex_size(je_tileset_document_t* doc, je_source_id_t source_idx,
    /* OPTIONAL */ int32_t* tex_w, /* OPTIONAL */ int32_t* tex_h)
{
    if (source_idx < 0 || source_idx >= (int32_t)doc->sources.size())
        return false;
    const auto& s = doc->sources[source_idx];
    if (tex_w != nullptr) *tex_w = s.tex_w.value_or(0);
    if (tex_h != nullptr) *tex_h = s.tex_h.value_or(0);
    return true;
}
/* OPTIONAL: If source_idx is invalid */ const int32_t*
    je_tilemap_source_cell_ids(je_tileset_document_t* doc, je_source_id_t source_idx,
        /* OPTIONAL */ int32_t* out_count)
{
    if (source_idx < 0 || source_idx >= (int32_t)doc->sources.size())
        return nullptr;
    const auto& s = doc->sources[source_idx];
    if (out_count != nullptr)
        *out_count = (int32_t)s.cell_ids.size();
    return s.cell_ids.data();
}

// ---------------- 图集源纹理附加层级 ----------------
int32_t je_tilemap_source_extra_count(je_tileset_document_t* doc, je_source_id_t source_idx)
{
    if (source_idx < 0 || source_idx >= (int32_t)doc->sources.size())
        return 0;
    return (int32_t)doc->sources[source_idx].extras.size();
}
bool je_tilemap_source_extra_add(je_tileset_document_t* doc, je_source_id_t source_idx,
    const char* texture_path, int32_t pass)
{
    assert(texture_path != nullptr);
    if (source_idx < 0 || source_idx >= (int32_t)doc->sources.size())
        return false;
    auto& src = doc->sources[source_idx];
    if (texture_path[0] == '\0' || pass < 1)
        return false;
    // 同源内通道唯一：同 (路径, 通道) 幂等返回 true；同通道不同路径或
    // 同路径不同通道（换通道应走 set_source_extra_pass）均拒绝
    for (const auto& e : src.extras)
    {
        if (e.pass == pass)
            return e.path == texture_path;
        if (e.path == texture_path)
            return false;
    }
    // 读取纹理尺寸：层级 cell 尺寸由它与主纹理网格推得（网格必须容得下）
    int32_t tw = 0, th = 0;
    if (auto tex = jeecs::graphic::texture2d::load(nullptr, texture_path))
    {
        tw = (int32_t)tex->get()->resource()->m_width;
        th = (int32_t)tex->get()->resource()->m_height;
    }
    else
    {
        jeecs::debug::logerr(
            "Tilemap: unable to load source extra texture '%s'.", texture_path);
        return false;
    }
    if (src.xcount <= 0 || src.ycount <= 0 || tw / src.xcount < 1 || th / src.ycount < 1)
    {
        jeecs::debug::logerr(
            "Tilemap: extra texture '%s' (%dx%d) too small for grid %dx%d.",
            texture_path, tw, th, src.xcount, src.ycount);
        return false;
    }
    jeecs::Tilemap::SourceExtra extra;
    extra.pass = pass;
    extra.path = texture_path;
    extra.tex_w = tw;
    extra.tex_h = th;
    // 展开为与主纹理层号对齐的数组纹理（分片按 pass 绑定采样）
    if (!jeecs::Tilemap::build_extra_array(src, extra))
        jeecs::debug::logwarn(
            "Tilemap: texture array for extra '%s' deferred (image not ready).",
            texture_path);
    src.extras.push_back(std::move(extra));
    ++doc->version;
    return true;
}
bool je_tilemap_source_extra_remove(je_tileset_document_t* doc,
    je_source_id_t source_idx, int32_t extra_idx)
{
    if (source_idx < 0 || source_idx >= (int32_t)doc->sources.size())
        return false;
    auto& extras = doc->sources[source_idx].extras;
    if (extra_idx < 0 || extra_idx >= (int32_t)extras.size())
        return false;
    extras.erase(extras.begin() + extra_idx);
    ++doc->version;
    return true;
}
bool je_tilemap_source_extra_info(je_tileset_document_t* doc, je_source_id_t source_idx,
    int32_t extra_idx, /* OPTIONAL */ int32_t* pass, /* OPTIONAL */ const char** texture_path)
{
    if (source_idx < 0 || source_idx >= (int32_t)doc->sources.size())
        return false;
    const auto& extras = doc->sources[source_idx].extras;
    if (extra_idx < 0 || extra_idx >= (int32_t)extras.size())
        return false;
    if (pass != nullptr) *pass = extras[extra_idx].pass;
    if (texture_path != nullptr) *texture_path = extras[extra_idx].path.c_str();
    return true;
}
bool je_tilemap_source_extra_tex_size(je_tileset_document_t* doc,
    je_source_id_t source_idx, int32_t extra_idx,
    /* OPTIONAL */ int32_t* tex_w, /* OPTIONAL */ int32_t* tex_h)
{
    if (source_idx < 0 || source_idx >= (int32_t)doc->sources.size())
        return false;
    const auto& extras = doc->sources[source_idx].extras;
    if (extra_idx < 0 || extra_idx >= (int32_t)extras.size())
        return false;
    if (tex_w != nullptr) *tex_w = extras[extra_idx].tex_w.value_or(0);
    if (tex_h != nullptr) *tex_h = extras[extra_idx].tex_h.value_or(0);
    return true;
}
bool je_tilemap_source_extra_pass_set(je_tileset_document_t* doc,
    je_source_id_t source_idx, int32_t extra_idx, int32_t pass)
{
    if (source_idx < 0 || source_idx >= (int32_t)doc->sources.size())
        return false;
    auto& extras = doc->sources[source_idx].extras;
    if (extra_idx < 0 || extra_idx >= (int32_t)extras.size() || pass < 1)
        return false;
    // 同源内通道唯一（与添加口径一致）
    for (size_t i = 0; i < extras.size(); ++i)
        if ((int32_t)i != extra_idx && extras[i].pass == pass)
            return false;
    if (extras[extra_idx].pass == pass)
        return true;
    extras[extra_idx].pass = pass;
    ++doc->version;
    return true;
}

// ---------------- 瓦片表（图集每个格都是一块瓦片） ----------------
// 瓦片 id 由图集源分配（添加源时给定基址），格与 id 的对应关系稳定；
// 通行性/名称/属性/自动拼接类型都是 id 上的稀疏元数据，无需"注册"。
static jeecs::Tilemap::TileDef* je_tilemap_tile_def(
    je_tileset_document_t* doc, je_tile_id_t tile_id, bool create)
{
    if (doc == nullptr || !doc->tile_cell(tile_id).has_value())
        return nullptr;
    if (create)
        return &doc->ensure_tile_def(tile_id);
    auto fnd = doc->tiles.find(tile_id);
    return fnd == doc->tiles.end() ? nullptr : &fnd->second;
}
int32_t je_tilemap_tile_count(je_tileset_document_t* doc)
{
    int32_t count = 0;
    for (const auto& s : doc->sources)
        count += s.xcount * s.ycount;
    return count;
}
bool je_tilemap_tile_at_index(je_tileset_document_t* doc, int32_t index, je_tile_id_t* out_id)
{
    assert(out_id != nullptr);
    if (index < 0)
        return false;
    int32_t offset = index;
    for (const auto& s : doc->sources)
    {
        const int32_t count = (int32_t)s.cell_ids.size();
        if (offset < count)
        {
            const int32_t id = s.cell_ids[(size_t)offset];
            if (id <= 0)
                return false;
            *out_id = id;
            return true;
        }
        offset -= count;
    }
    return false;
}
int32_t je_tilemap_tile_defs(je_tileset_document_t* doc,
    /* OPTIONAL */ je_tile_id_t* out_ids, int32_t capacity)
{
    int32_t written = 0;
    for (const auto& entry : doc->tiles)
    {
        if (out_ids != nullptr && written < capacity)
            out_ids[written] = entry.first;
        ++written;
    }
    return written;
}
int32_t je_tilemap_tile_def_property_count(je_tileset_document_t* doc, je_tile_id_t tile_id)
{
    const auto* def = doc->tile_def(tile_id);
    return def == nullptr ? 0 : (int32_t)def->properties.size();
}
bool je_tilemap_tile_def_property_info(je_tileset_document_t* doc, je_tile_id_t tile_id,
    int32_t index, /* OPTIONAL */ const char** name,
    /* OPTIONAL */ je_property_type_t* type)
{
    const auto* def = doc->tile_def(tile_id);
    if (def == nullptr || index < 0 || index >= (int32_t)def->properties.size())
        return false;

    auto it = def->properties.begin();
    std::advance(it, index);
    if (name != nullptr)
        *name = it->first.c_str();
    if (type != nullptr)
        *type = (je_property_type_t)it->second.index();
    return true;
}
bool je_tilemap_tile_id_at(je_tileset_document_t* doc, je_source_id_t source_idx,
    int32_t ix, int32_t iy, je_tile_id_t* out_id)
{
    assert(out_id != nullptr);
    const int32_t tile_id = doc->tile_id_at(source_idx, ix, iy);
    if (tile_id <= 0)
        return false;
    *out_id = tile_id;
    return true;
}
bool je_tilemap_tile_info(je_tileset_document_t* doc, je_tile_id_t tile_id,
    /* OPTIONAL */ je_source_id_t* source_idx, /* OPTIONAL */ int32_t* ix, /* OPTIONAL */ int32_t* iy, /* OPTIONAL */ int32_t* walkable)
{
    const auto hit = doc->tile_cell(tile_id);
    if (!hit.has_value())
        return false;
    if (source_idx != nullptr) *source_idx = std::get<0>(*hit);
    if (ix != nullptr) *ix = std::get<1>(*hit);
    if (iy != nullptr) *iy = std::get<2>(*hit);
    if (walkable != nullptr) *walkable = doc->tile_walkable(tile_id) ? 1 : 0;
    return true;
}
const char* je_tilemap_tile_name(je_tileset_document_t* doc, je_tile_id_t tile_id)
{
    const auto* def = je_tilemap_tile_def(doc, tile_id, false);
    return def == nullptr ? "" : def->name.c_str();
}
bool je_tilemap_tile_name_set(je_tileset_document_t* doc, je_tile_id_t tile_id, const char* name)
{
    assert(name != nullptr);
    auto* def = je_tilemap_tile_def(doc, tile_id, true);
    if (def == nullptr)
        return false;
    if (def->name != name)
    {
        def->name = name;
        doc->compact_tile_def(tile_id);
        ++doc->version;
    }
    else
        doc->compact_tile_def(tile_id);   // 可能是新建的空条目
    return true;
}
je_tile_kind_t je_tilemap_tile_kind(je_tileset_document_t* doc, je_tile_id_t tile_id)
{
    return doc->tile_kind(tile_id);
}
bool je_tilemap_tile_kind_set(je_tileset_document_t* doc, je_tile_id_t tile_id, je_tile_kind_t kind)
{
    if (kind < JE_TILE_KIND_PLAIN || kind > JE_TILE_KIND_RPGMAKER_VX
        || !doc->tile_cell(tile_id).has_value())
        return false;
    auto* def = je_tilemap_tile_def(doc, tile_id, true);
    if (def->kind != kind)
    {
        def->kind = kind;
        doc->compact_tile_def(tile_id);
        ++doc->version;
    }
    else
        doc->compact_tile_def(tile_id);   // 可能是新建的空条目
    return true;
}
int32_t je_tilemap_tile_variant_count(je_tileset_document_t* doc, je_tile_id_t tile_id)
{
    const auto* def = je_tilemap_tile_def(doc, tile_id, false);
    return def == nullptr ? 0 : (int32_t)def->variant_tiles.size();
}
bool je_tilemap_tile_variant(je_tileset_document_t* doc, je_tile_id_t tile_id,
    int32_t index, je_tile_id_t* out_variant)
{
    assert(out_variant != nullptr);
    const auto* def = je_tilemap_tile_def(doc, tile_id, false);
    if (def == nullptr || index < 0 || index >= (int32_t)def->variant_tiles.size())
        return false;
    *out_variant = def->variant_tiles[index];
    return true;
}
bool je_tilemap_tile_variant_clear(je_tileset_document_t* doc, je_tile_id_t tile_id)
{
    auto* def = je_tilemap_tile_def(doc, tile_id, false);
    if (def == nullptr || def->variant_tiles.empty())
        return def != nullptr;
    def->variant_tiles.clear();
    doc->compact_tile_def(tile_id);
    ++doc->version;
    return true;
}
bool je_tilemap_tile_variant_add(je_tileset_document_t* doc, je_tile_id_t tile_id,
    je_tile_id_t variant_tile)
{
    auto* def = je_tilemap_tile_def(doc, tile_id, true);
    if (def == nullptr)
        return false;
    if (!doc->tile_cell(variant_tile).has_value())
    {
        doc->compact_tile_def(tile_id);
        return false;
    }
    def->variant_tiles.push_back(variant_tile);
    ++doc->version;
    return true;
}
void je_tilemap_tile_walkable_set(je_tileset_document_t* doc, je_tile_id_t tile_id, int32_t walkable)
{
    auto* def = je_tilemap_tile_def(doc, tile_id, true);
    if (def == nullptr)
        return;
    const bool value = walkable != 0;
    if (def->walkable != value)
    {
        def->walkable = value;
        doc->compact_tile_def(tile_id);
        ++doc->version;
    }
    else
        doc->compact_tile_def(tile_id);   // 可能是新建的空条目
}

// ---------------- 瓦片法线（参与网格顶点生成） ----------------
bool je_tilemap_tile_normal(je_tileset_document_t* doc, je_tile_id_t tile_id,
    /* OPTIONAL */ float* out_x, /* OPTIONAL */ float* out_y, /* OPTIONAL */ float* out_z)
{
    if (!doc->tile_cell(tile_id).has_value())
        return false;
    const auto* def = je_tilemap_tile_def(doc, tile_id, false);
    const auto& normal = def == nullptr
        ? jeecs::Tilemap::DEFAULT_TILE_NORMAL : def->normal;
    if (out_x != nullptr) *out_x = normal[0];
    if (out_y != nullptr) *out_y = normal[1];
    if (out_z != nullptr) *out_z = normal[2];
    return true;
}
bool je_tilemap_tile_normal_set(je_tileset_document_t* doc, je_tile_id_t tile_id,
    float x, float y, float z)
{
    // 法线必须能归一化：零长/非有限分量拒绝（否则顶点法线无意义）
    const float len = std::sqrt(x * x + y * y + z * z);
    if (!std::isfinite(len) || len <= 0.f)
        return false;
    auto* def = je_tilemap_tile_def(doc, tile_id, true);
    if (def == nullptr)
        return false;
    const std::array<float, 3> normal{ x / len, y / len, z / len };
    if (def->normal != normal)
    {
        def->normal = normal;
        doc->compact_tile_def(tile_id);
        ++doc->version;
    }
    else
        doc->compact_tile_def(tile_id);   // 可能是新建的空条目
    return true;
}

// ---------------- 属性 schema 与默认值 ----------------
bool je_tilemap_property_add(je_tileset_document_t* doc, const char* name,
    je_property_type_t type, /* OPTIONAL */ const void* default_value, int32_t* out_index)
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
bool je_tilemap_property_remove(je_tileset_document_t* doc, const char* name)
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
int32_t je_tilemap_property_count(je_tileset_document_t* doc)
{
    return (int32_t)doc->properties.size();
}
bool je_tilemap_property_info(je_tileset_document_t* doc, int32_t index,
    /* OPTIONAL */ const char** name, /* OPTIONAL */ je_property_type_t* type)
{
    if (index < 0 || index >= (int32_t)doc->properties.size())
        return false;
    const auto& pd = doc->properties[index];
    if (name != nullptr) *name = pd.name.c_str();
    if (type != nullptr) *type = pd.type;
    return true;
}
bool je_tilemap_property_default(je_tileset_document_t* doc, const char* name,
    je_property_type_t type, /* OPTIONAL */ void* out_value)
{
    assert(name != nullptr);
    assert(out_value != nullptr);
    for (const auto& pd : doc->properties)
        if (pd.name == name)
            return je_tilemap_read_property(pd.default_value, type, out_value);
    return false;
}
bool je_tilemap_tile_property_set(je_tileset_document_t* doc, je_tile_id_t tile_id,
    const char* name, je_property_type_t type, /* OPTIONAL */ const void* value)
{
    assert(name != nullptr);
    if (!jeecs::Tilemap::property_type_valid((int32_t)type))
        return false;
    if (value == nullptr)
    {
        // 清除默认值：忽略 type，未设置过也视为成功
        auto* def = je_tilemap_tile_def(doc, tile_id, false);
        if (def == nullptr)
            return false;
        if (def->properties.erase(name) > 0)
        {
            doc->compact_tile_def(tile_id);
            ++doc->version;
        }
        return true;
    }
    auto* def = je_tilemap_tile_def(doc, tile_id, true);
    if (def == nullptr)
        return false;
    auto fnd = def->properties.find(name);
    if (fnd != def->properties.end() && fnd->second.index() != (size_t)type)
    {
        // 已有不同类型的值，拒绝改型（新建的空条目随之清理）
        doc->compact_tile_def(tile_id);
        return false;
    }
    jeecs::Tilemap::property_value v;
    if (!je_tilemap_write_property(v, type, value))
    {
        doc->compact_tile_def(tile_id);
        return false;
    }
    def->properties[name] = std::move(v);
    ++doc->version;
    return true;
}
bool je_tilemap_tile_property_get(je_tileset_document_t* doc, je_tile_id_t tile_id,
    const char* name, je_property_type_t type, /* OPTIONAL */ void* out_value)
{
    assert(name != nullptr);
    assert(out_value != nullptr);
    const auto* def = je_tilemap_tile_def(doc, tile_id, false);
    if (def == nullptr)
        return false;
    auto fnd = def->properties.find(name);
    return fnd == def->properties.end()
        ? false
        : je_tilemap_read_property(fnd->second, type, out_value);
}

// ======================================================================
// 单元格渲染信息（编辑器预览用）：resolve_cell_quads 的 C 边界映射
// ======================================================================
int32_t je_tilemap_cell_quad_count(je_tilemap_document_t* doc, je_layer_id_t layer, int32_t x, int32_t y)
{
    jeecs::Tilemap::cell_quad_info quads[4];
    return jeecs::Tilemap::resolve_cell_quads(*doc, layer, x, y, quads);
}
bool je_tilemap_cell_quad(je_tilemap_document_t* doc, je_layer_id_t layer, int32_t x, int32_t y, int32_t idx,
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
