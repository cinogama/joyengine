#pragma once

// UI 文本逐字渲染器（引擎内部实现，不属于公共 C-API）。
//
// 与旧的 text_texture_impl（整段文本 CPU 逐像素合成一张纹理）的本质区别：
// 文本永不构成纹理。每个可见字符都是独立四边形面片，逐字携带位置 / UV /
// 颜色，批量写入共享动态顶点缓冲后按连续段绘制（见
// UserInterfaceGraphicPipelineSystem 的接入代码）。
//
// 核心不变量：
//  * 字形按（字体, 量化字号, 码点）经 je_font_get_char 光栅化一次（复用其
//    内部缓存），装箱进按字体分页的 RGBA 图集（白色 + 覆盖度 alpha）。
//    页只增不减（append-only），已有字形的 UV 永不失效，无需重排；图集
//    与文本内容无关，全场景共享；
//  * 行网格：行高仅取决于字体与基准字号（与文本内容、{scale:} 标记无关），
//    字形按基线相对放置（缩放字形向基线上方生长）——基线稳定、无上下
//    跳变；垂直对齐锚定字体度量行盒（ascent..descent）而非墨水包围盒，
//    有无降部字符不改变行盒位置；
//  * 布局缓存存于 Text::runtime_layout，脏检查键为
//    （内容, 字体, 量化字号, 折行宽度, 对齐）；修改颜色 / 缩放 / 偏移
//    标记是零成本的（逐字实时属性，不触发重光栅化）；
//  * 全部方法必须在逻辑帧窗口内（图形线程静默期）调用。

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif

#include "jeecs.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <unordered_map>
#include <vector>

namespace jeecs
{
    class ui_text_renderer final
    {
    public:
        // 图集页边长（像素，RGBA）。典型 CJK 字体在常用字号下 1-2 页即可
        // 覆盖；页数受 MAX_TOTAL_PAGES 约束。
        static constexpr size_t PAGE_SIZE = 1024;
        // 字形装箱间隔：防止相邻字形经线性采样互相渗色（UV 另有半像素内缩）。
        static constexpr size_t PAGE_GUTTER = 1;
        // 全局（跨字体）页数上限，超限淘汰最久未用页。
        static constexpr size_t MAX_TOTAL_PAGES = 32;
        // 字体条目闲置超过该帧数后整组释放（同时释放字体引用）。
        static constexpr uint64_t FONT_IDLE_FRAMES = 600;
        // 单字形光栅字号上限：避免病态 {scale:} 触发巨幅位图光栅化
        //（超过上限的字形按上限字号光栅，仅度量受影响）。
        static constexpr int MAX_RASTER_PX = 512;

        // 图集页：一张 RGBA 纹理 + 行式装箱游标。存于 std::list——页淘汰
        // 只使被删页失效，其余页（及指向它们的字形槽）地址稳定。
        struct page_t
        {
            basic::resource<graphic::texture> m_texture;
            size_t m_cursor_x = PAGE_GUTTER;
            size_t m_cursor_y = PAGE_GUTTER;
            size_t m_row_height = 0;
            uint64_t m_last_used_frame = 0;

            explicit page_t(basic::resource<graphic::texture>&& tex) noexcept
                : m_texture(std::move(tex))
            {
            }
        };

        // 图集中的一个字形：UV（含半像素内缩）+ 布局度量。布局（量宽）与
        // 发射（取 UV）共用；无位图的字形（空格等，w/h 为 0）只保留度量。
        struct glyph_slot
        {
            float u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f;
            int16_t w = 0, h = 0;
            int16_t baseline_offset_x = 0, baseline_offset_y = 0;
            float advance_x = 0.f;
            page_t* page = nullptr; // 所属图集页；无位图为 nullptr
        };

        // 行网格度量：仅取决于（字体, 基准字号），与文本内容无关。
        struct font_metrics_t
        {
            float line_height = 0.f; // 行距：基线到基线
            float ascent = 0.f;      // 基线以上部分（像素）
            float descent = 0.f;     // 基线以下部分（像素，正值）
        };

        // 每帧首调用：推进帧计数并做 LRU 淘汰。
        void begin_frame() noexcept
        {
            ++m_frame;

            // 闲置字体整组释放（含字形、页与字体引用）。
            for (auto it = m_fonts.begin(); it != m_fonts.end();)
            {
                if (m_frame - it->second.m_last_used_frame > FONT_IDLE_FRAMES)
                {
                    m_total_pages -= it->second.m_pages.size();
                    it = m_fonts.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            // 页数超限兜底：全局淘汰最久未用页。正常场景达不到上限；
            // 达到时说明活跃字形总量确实超过 32 页，淘汰后字形会在下次
            // 解析时自动重建（重建风暴时告警一次）。
            while (m_total_pages > MAX_TOTAL_PAGES)
            {
                if (!_evict_one_lru_page())
                    break;
            }
        }

        // 解析（必要时光栅化并装箱）一个字形。同帧内返回的指针稳定；
        // 跨帧不保证（页可能被淘汰），发射端应每帧重新解析。
        const glyph_slot* resolve(
            const basic::resource<graphic::font>& font,
            int raster_px, char32_t cp)
        {
            assert(raster_px > 0);

            auto& entry = m_fonts[font->resource()];
            if (!entry.m_font.has_value() || entry.m_font.value().get() != font.get())
                entry.m_font.emplace(font);
            entry.m_last_used_frame = m_frame;

            const uint64_t key =
                (uint64_t)(uint32_t)raster_px << 32 | (uint64_t)(uint32_t)cp;
            if (auto fnd = entry.m_glyphs.find(key); fnd != entry.m_glyphs.end())
            {
                if (fnd->second.page != nullptr)
                    fnd->second.page->m_last_used_frame = m_frame;
                return &fnd->second;
            }
            return &_create_glyph(entry, raster_px, cp, key);
        }

        // 行网格度量。行距取自任意字形的 m_advance_y（同一字号下全字符
        // 一致），像素换算比例由 行距/字体行高单位 推出，ascent/descent
        // 随之确定——全程无需新增字体 C-API。
        static font_metrics_t font_metrics(graphic::font& f, int font_px) noexcept
        {
            font_metrics_t m;
            const auto* jf = f.resource();

            if (jf->m_line_space != 0)
            {
                const auto* space = f.get_character((float)font_px, U' ');
                m.line_height =
                    space != nullptr ? (float)(-space->m_advance_y) : (float)font_px;
                const float scale = m.line_height / (float)jf->m_line_space;
                m.ascent = (float)jf->m_ascent * scale;
                m.descent = -(float)jf->m_descent * scale;
            }
            else
            {
                // 病态字体（无行高度量）的兜底：按典型比例近似。
                m.line_height = (float)font_px;
                m.ascent = 0.8f * m.line_height;
                m.descent = 0.2f * m.line_height;
            }
            return m;
        }

        // 文本块左下角在矩形局部系（原点=矩形中心）中的位置：
        // 按对齐位掩码做九宫格放置（块不拉伸，溢出对称外扩）。
        static math::vec2 block_origin_local(
            const UserInterface::Element::resolved_rect& rect,
            const UserInterface::Text::runtime_layout_t& layout,
            UserInterface::Element::alignment align,
            float camera_scale) noexcept
        {
            using Element = UserInterface::Element;

            const float bw = layout.block_w * camera_scale;
            const float bh = layout.block_h * camera_scale;

            float h_off = (rect.size.x - bw) * 0.5f;
            if (0 != (align & Element::alignment::left))
                h_off = 0.f;
            else if (0 != (align & Element::alignment::right))
                h_off = rect.size.x - bw;

            float v_off = (rect.size.y - bh) * 0.5f;
            if (0 != (align & Element::alignment::bottom))
                v_off = 0.f;
            else if (0 != (align & Element::alignment::top))
                v_off = rect.size.y - bh;

            return math::vec2(
                -rect.size.x * 0.5f + h_off,
                -rect.size.y * 0.5f + v_off);
        }

        // 布局一段文本（标记解析 → 字形度量 → 折行 → 对齐 → 放置），
        // 结果写入 Text::runtime_layout。仅当脏检查键变化时重建；
        // 字体缺失 / 基准轴无尺寸时缓存置为无效（不渲染）。
        //
        // 文本空间约定：原点=块左下角，y 向上，单位=基准字号像素；
        // win_w/win_h 为布局参照分辨率（窗口尺寸），与
        // RasterizeDirtyTexts 旧路径的字号推导一致：基准轴取
        // width_unit 的 x 分量，否则取 y 分量（per_axis 亦取 y，
        // 文本以字高为基准更自然）。
        void layout_text(
            UserInterface::Text& text,
            const UserInterface::Element& elem,
            const UserInterface::WorldLayout& world,
            float win_w, float win_h)
        {
            using Element = UserInterface::Element;
            auto& cache = text.runtime_layout;

            const auto invalidate = [&cache]() noexcept
                {
                    cache.valid = false;
                    cache.glyphs.clear();
                    cache.block_w = cache.block_h = 0.f;
                };

            const auto rect = elem.resolve_layout(world, win_w, win_h);
            const float base =
                elem.unit_kind == Element::ratio_unit::width_unit
                ? rect.size.x : rect.size.y;
            if (base <= 0.f)
                return invalidate();

            auto font_holder = text.font.get_resource();
            if (!font_holder.has_value())
                return invalidate();
            auto& font_res = font_holder.value();

            const int font_px = (int)std::lround(base);
            const int wrap_w =
                text.wrap == UserInterface::Text::wrap_mode::word && !text.auto_size
                ? (int)std::lround(rect.size.x) : -1;

            // 脏检查：键完全一致则沿用上一帧布局。
            if (cache.valid
                && cache.key_font == font_res->resource()
                && cache.key_font_px == font_px
                && cache.key_wrap_width_px == wrap_w
                && cache.key_alignment == (uint32_t)text.alignment
                && cache.key_content == text.content)
            {
                return;
            }

            invalidate();

            const font_metrics_t fm = font_metrics(*font_res.get(), font_px);
            if (fm.line_height <= 0.f)
                return;

            // ---- 1) 标记解析 → 带样式的字符流 ----
            std::vector<styled_char_t> chars;
            _parse_markup(text.content.cpp_str(), chars);
            const size_t n = chars.size();

            // ---- 2) 逐字符解析字形（度量 + 装入图集）----
            std::vector<const glyph_slot*> slots(n, nullptr);
            std::vector<int> raster_px(n, 1);
            std::vector<float> advance(n, 0.f);
            for (size_t i = 0; i < n; ++i)
            {
                raster_px[i] = std::clamp(
                    (int)std::lround((float)font_px * chars[i].scale),
                    1, MAX_RASTER_PX);
                slots[i] = resolve(font_res, raster_px[i], chars[i].cp);
                advance[i] = slots[i]->advance_x;
            }

            // ---- 3) 折行 → 行区间（begin/end 独占）与行宽（剔除行尾空格）----
            struct line_span_t { size_t begin, end; float width; };
            std::vector<line_span_t> lines;

            const auto push_line = [&](size_t begin, size_t end)
                {
                    float width = 0.f;
                    // 行尾空格不计宽（断行机会由空格产生时，该空格已在
                    // end 之前被跳过；此处剔除行内残留的尾随空格）。
                    while (end > begin && _is_space(chars[end - 1].cp))
                        --end;
                    for (size_t j = begin; j < end; ++j)
                        width += advance[j];
                    lines.push_back(line_span_t{ begin, end, width });
                };

            constexpr auto NONE = SIZE_MAX;
            size_t line_begin = 0, last_opp = NONE, i = 0;
            float pen = 0.f;
            const float wrap_limit = (float)(wrap_w > 0 ? wrap_w : 0);

            while (i < n)
            {
                const char32_t cp = chars[i].cp;

                if (cp == U'\n')
                {
                    push_line(line_begin, i);
                    line_begin = ++i;
                    pen = 0.f;
                    last_opp = NONE;
                    continue;
                }

                if (wrap_w > 0 && i > line_begin && pen + advance[i] > wrap_limit)
                {
                    if (last_opp != NONE && last_opp > line_begin && last_opp <= i)
                    {
                        // 在最近断行机会处折行（机会处的空格被顺带消费）。
                        push_line(line_begin, last_opp);
                        line_begin = last_opp;
                    }
                    else
                    {
                        // 行内无断行机会：硬断在当前字符前（单字符超宽时
                        // 下一轮 i == line_begin，即使超宽也会被放下）。
                        push_line(line_begin, i);
                        line_begin = i;
                    }
                    pen = 0.f;
                    for (size_t j = line_begin; j < i; ++j)
                        pen += advance[j];
                    last_opp = NONE;
                    continue; // 重处理当前字符（可能再次触发溢出→硬断）
                }

                pen += advance[i];

                // 断行机会：空格后、CJK 后、CJK 前。
                if (_is_space(cp) || _is_breakable(cp))
                    last_opp = i + 1;
                else if (i + 1 < n && _is_breakable(chars[i + 1].cp))
                    last_opp = i + 1;

                ++i;
            }
            push_line(line_begin, n);

            // ---- 4) 对齐放置 ----
            float block_w = 0.f;
            for (const auto& line : lines)
                block_w = std::max(block_w, line.width);

            const size_t line_count = lines.size();
            const float block_h =
                fm.ascent + fm.descent + (float)(line_count - 1) * fm.line_height;

            const bool align_left = 0 != (text.alignment & Element::alignment::left);
            const bool align_right = 0 != (text.alignment & Element::alignment::right);

            cache.glyphs.clear();
            for (size_t k = 0; k < line_count; ++k)
            {
                const auto& line = lines[k];
                // 第 k 行基线（y 向上，原点=块左下角）：
                //   block_h − ascent − k·line_height
                const float baseline_y =
                    block_h - fm.ascent - (float)k * fm.line_height;

                float x0 = (block_w - line.width) * 0.5f;
                if (align_left)
                    x0 = 0.f;
                else if (align_right)
                    x0 = block_w - line.width;

                float pen_x = 0.f;
                for (size_t j = line.begin; j < line.end; ++j)
                {
                    const glyph_slot* slot = slots[j];
                    if (slot->w > 0 && slot->h > 0)
                    {
                        // 字形盒左下角 = 笔位 + 基线偏移 + offset 标记
                        //（offset 单位为基准字号倍数，y 向上）。取整保证
                        // 1:1 像素映射下字形锐利；基线 y 已在整数网格上。
                        UserInterface::Text::runtime_layout_t::glyph_t glyph;
                        glyph.cp = chars[j].cp;
                        glyph.raster_px = raster_px[j];
                        glyph.x = std::round(
                            pen_x + (float)slot->baseline_offset_x
                            + chars[j].offx * (float)font_px);
                        glyph.y = std::round(
                            baseline_y + (float)slot->baseline_offset_y
                            + chars[j].offy * (float)font_px);
                        glyph.color = chars[j].color;
                        cache.glyphs.push_back(glyph);
                    }
                    pen_x += advance[j];
                }
            }

            // ---- 5) 写入脏检查键与布局结果 ----
            cache.key_font = font_res->resource();
            cache.key_font_px = font_px;
            cache.key_wrap_width_px = wrap_w;
            cache.key_alignment = (uint32_t)text.alignment;
            cache.key_content = text.content;
            cache.block_w = block_w;
            cache.block_h = block_h;
            cache.win_base_px = base;
            cache.valid = true;
        }

    private:
        struct styled_char_t
        {
            char32_t cp;
            math::vec4 color;
            float scale;
            float offx, offy;
        };

        struct font_atlas_t
        {
            basic::optional<basic::resource<graphic::font>> m_font; // 持有字体引用，键存活期内稳定
            std::list<page_t> m_pages;
            std::unordered_map<uint64_t, glyph_slot> m_glyphs; // key = (size<<32)|cp
            uint64_t m_last_used_frame = 0;
        };

        std::unordered_map<je_font*, font_atlas_t> m_fonts;
        size_t m_total_pages = 0;
        uint64_t m_frame = 0;
        bool m_warned_page_eviction = false;

        glyph_slot& _create_glyph(
            font_atlas_t& entry, int raster_px, char32_t cp, uint64_t key)
        {
            const auto* ch = entry.m_font.value()->get_character(
                (float)raster_px, cp);

            glyph_slot slot;
            if (ch != nullptr)
            {
                slot.advance_x = (float)ch->m_advance_x;
                slot.baseline_offset_x = (int16_t)ch->m_baseline_offset_x;
                slot.baseline_offset_y = (int16_t)ch->m_baseline_offset_y;
                slot.w = (int16_t)ch->m_width;
                slot.h = (int16_t)ch->m_height;

                if (slot.w > 0 && slot.h > 0
                    && (size_t)slot.w + 2 * PAGE_GUTTER <= PAGE_SIZE
                    && (size_t)slot.h + 2 * PAGE_GUTTER <= PAGE_SIZE)
                {
                    _pack_glyph(entry, slot, *ch->m_texture.get());
                }
            }
            return entry.m_glyphs.emplace(key, slot).first->second;
        }

        // 行式装箱：当前行放不下则换行，页满则开新页。页只增不减，
        // 因此无需重排、已有字形 UV 永不失效。
        void _pack_glyph(
            font_atlas_t& entry, glyph_slot& slot, graphic::texture& src)
        {
            for (;;)
            {
                if (!entry.m_pages.empty())
                {
                    page_t& page = entry.m_pages.back();

                    if (page.m_cursor_x + (size_t)slot.w > PAGE_SIZE - PAGE_GUTTER)
                    {
                        page.m_cursor_y += page.m_row_height + PAGE_GUTTER;
                        page.m_cursor_x = PAGE_GUTTER;
                        page.m_row_height = 0;
                    }
                    if (page.m_cursor_y + (size_t)slot.h <= PAGE_SIZE - PAGE_GUTTER)
                    {
                        _blit_glyph(page, slot, src);
                        return;
                    }
                }
                _new_page(entry);
            }
        }

        void _blit_glyph(page_t& page, glyph_slot& slot, graphic::texture& src)
        {
            const size_t gw = (size_t)slot.w, gh = (size_t)slot.h;
            const size_t px = page.m_cursor_x, py = page.m_cursor_y;

            auto* dst_res = page.m_texture->resource();
            const auto* src_res = src.resource();

            // 源为 je_font_get_char 生成的 RGBA 字形纹理（白色 + 覆盖度
            // alpha，自底向上行序）；图集页同序，逐行拷贝 alpha、RGB 写白。
            for (size_t row = 0; row < gh; ++row)
            {
                uint8_t* dst = dst_res->m_pixels + ((py + row) * PAGE_SIZE + px) * 4;
                const uint8_t* s = src_res->m_pixels + row * gw * 4;
                for (size_t col = 0; col < gw; ++col, dst += 4, s += 4)
                {
                    dst[0] = dst[1] = dst[2] = 0xFF;
                    dst[3] = s[3];
                }
            }
            // 后端按整纹理重传（m_modified 范围字段仅为建议），因此新字形
            // 出现时该页整体上传一次，其余时间零纹理上传。
            dst_res->m_handle.m_modified = true;

            // UV 半像素内缩（逐轴，1px 字形不内缩）：避免相邻字形 / 页边缘
            // 经线性采样渗色。
            const float inset_x = gw > 1 ? 0.5f : 0.f;
            const float inset_y = gh > 1 ? 0.5f : 0.f;
            slot.u0 = ((float)px + inset_x) / (float)PAGE_SIZE;
            slot.v0 = ((float)py + inset_y) / (float)PAGE_SIZE;
            slot.u1 = ((float)(px + gw) - inset_x) / (float)PAGE_SIZE;
            slot.v1 = ((float)(py + gh) - inset_y) / (float)PAGE_SIZE;

            slot.page = &page;
            page.m_cursor_x += gw + PAGE_GUTTER;
            page.m_row_height = std::max(page.m_row_height, gh);
            page.m_last_used_frame = m_frame;
        }

        void _new_page(font_atlas_t& entry)
        {
            while (m_total_pages >= MAX_TOTAL_PAGES)
            {
                if (!_evict_one_lru_page())
                    break;
            }

            auto texture = graphic::texture::create(
                PAGE_SIZE, PAGE_SIZE, jegl_texture::format::RGBA);
            std::memset(
                texture->resource()->m_pixels, 0, PAGE_SIZE * PAGE_SIZE * 4);

            entry.m_pages.emplace_back(std::move(texture));
            ++m_total_pages;
        }

        // 全局淘汰一页（最久未用）。返回 false 表示已无页可淘汰。
        bool _evict_one_lru_page()
        {
            font_atlas_t* owner = nullptr;
            page_t* victim = nullptr;
            for (auto& [key, entry] : m_fonts)
            {
                for (auto& page : entry.m_pages)
                {
                    if (victim == nullptr
                        || page.m_last_used_frame < victim->m_last_used_frame)
                    {
                        victim = &page;
                        owner = &entry;
                    }
                }
            }
            if (owner == nullptr)
                return false;

            if (!m_warned_page_eviction)
            {
                m_warned_page_eviction = true;
                jeecs::debug::logwarn(
                    "ui_text_renderer: glyph atlas page limit (%zu) reached, "
                    "evicting least-recently-used pages; heavy glyph load "
                    "may cause re-rasterization.",
                    MAX_TOTAL_PAGES);
            }

            // 先删除指向该页的字形槽，再删除页本身。
            for (auto it = owner->m_glyphs.begin(); it != owner->m_glyphs.end();)
            {
                if (it->second.page == victim)
                    it = owner->m_glyphs.erase(it);
                else
                    ++it;
            }
            owner->m_pages.remove_if(
                [victim](const page_t& p) { return &p == victim; });
            --m_total_pages;
            return true;
        }

        // ---- 标记解析 ----

        // 拉丁空格 / 制表符按断行空白处理；全角空格（U+3000）落在
        // _is_breakable 的 CJK 判定内。
        static bool _is_space(char32_t c) noexcept
        {
            return c == U' ' || c == U'\t';
        }

        // CJK / 全角 / 假名 / 谚文等“任意字符间可断行”的近似范围集。
        static bool _is_breakable(char32_t c) noexcept
        {
            return (c >= 0x1100 && c <= 0x115F)   // 谚文字母
                || (c >= 0x2E80 && c <= 0x303E)   // CJK 部首、注音等
                || (c >= 0x3041 && c <= 0x33FF)   // 假名、CJK 兼容
                || (c >= 0x3400 && c <= 0x4DBF)   // CJK 扩展 A
                || (c >= 0x4E00 && c <= 0x9FFF)   // CJK 基本
                || (c >= 0xA000 && c <= 0xA4CF)   // 彝文
                || (c >= 0xAC00 && c <= 0xD7A3)   // 谚文音节
                || (c >= 0xF900 && c <= 0xFAFF)   // CJK 兼容表意
                || (c >= 0xFE30 && c <= 0xFE4F)   // CJK 兼容形式
                || (c >= 0xFF00 && c <= 0xFF60)   // 全角形式
                || (c >= 0xFFE0 && c <= 0xFFE6);  // 全角符号
        }

        static std::u32string _utf8_to_u32(const std::string& s)
        {
            std::u32string out;
            out.reserve(s.size());
            for (size_t i = 0; i < s.size();)
            {
                const uint8_t c = (uint8_t)s[i];
                char32_t cp = 0;
                size_t len = 0;
                if (c < 0x80) { cp = c; len = 1; }
                else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
                else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
                else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
                else { ++i; continue; } // 非法前导字节：跳过

                bool ok = i + len <= s.size();
                for (size_t k = 1; ok && k < len; ++k)
                {
                    const uint8_t cc = (uint8_t)s[i + k];
                    if ((cc & 0xC0) != 0x80)
                        ok = false;
                    else
                        cp = (cp << 6) | (char32_t)(cc & 0x3F);
                }
                if (!ok) { ++i; continue; } // 截断/非法序列：跳过

                out.push_back(cp);
                i += len;
            }
            return out;
        }

        static std::string _u32_to_utf8(const char32_t* s, size_t n)
        {
            std::string out;
            out.reserve(n);
            for (size_t i = 0; i < n; ++i)
            {
                const char32_t c = s[i];
                if (c < 0x80)
                {
                    out += (char)c;
                }
                else if (c < 0x800)
                {
                    out += (char)(0xC0 | (c >> 6));
                    out += (char)(0x80 | (c & 0x3F));
                }
                else if (c < 0x10000)
                {
                    out += (char)(0xE0 | (c >> 12));
                    out += (char)(0x80 | ((c >> 6) & 0x3F));
                    out += (char)(0x80 | (c & 0x3F));
                }
                else
                {
                    out += (char)(0xF0 | (c >> 18));
                    out += (char)(0x80 | ((c >> 12) & 0x3F));
                    out += (char)(0x80 | ((c >> 6) & 0x3F));
                    out += (char)(0x80 | (c & 0x3F));
                }
            }
            return out;
        }

        // 解析十六进制颜色（RRGGBBAA；短输入零填充，兼容旧标记行为）。
        static math::vec4 _parse_hex_color(const std::u32string& hex)
        {
            char buf[9] = "00000000";
            const size_t n = std::min<size_t>(hex.size(), 8);
            for (size_t i = 0; i < n; ++i)
                buf[i] = (char)hex[i];
            const unsigned int packed = (unsigned int)strtoul(buf, nullptr, 16);
            return math::vec4(
                (float)((packed >> 24) & 0xFF) / 255.f,
                (float)((packed >> 16) & 0xFF) / 255.f,
                (float)((packed >> 8) & 0xFF) / 255.f,
                (float)(packed & 0xFF) / 255.f);
        }

        // 单遍标记解析：{field:value} 属性段（绝对设置）+ '\' 转义。
        // 未闭合的 '{' 静默消费至结尾（与旧实现一致）。
        static void _parse_markup(
            const std::string& content, std::vector<styled_char_t>& out)
        {
            const std::u32string wtext = _utf8_to_u32(content);

            math::vec4 color(1.f, 1.f, 1.f, 1.f);
            float scale = 1.f, offx = 0.f, offy = 0.f;

            const auto emit_char = [&](char32_t cp)
                {
                    out.push_back(styled_char_t{ cp, color, scale, offx, offy });
                };

            for (size_t i = 0; i < wtext.size();)
            {
                const char32_t ch = wtext[i];

                if (ch == U'\\')
                {
                    // 转义下一个字符（行尾孤立 '\' 丢弃）。
                    if (++i < wtext.size())
                        emit_char(wtext[i]);
                    ++i;
                    continue;
                }

                if (ch == U'{')
                {
                    std::u32string field, value;
                    bool in_field = true, closed = false;
                    size_t j = i + 1;
                    for (; j < wtext.size(); ++j)
                    {
                        const char32_t c = wtext[j];
                        if (c == U':')
                            in_field = false;
                        else if (c == U'}')
                        {
                            closed = true;
                            break;
                        }
                        else if (in_field)
                            field += c;
                        else
                            value += c;
                    }

                    if (closed)
                    {
                        if (field == U"scale")
                        {
                            const float v = std::strtof(
                                _u32_to_utf8(value.c_str(), value.size()).c_str(),
                                nullptr);
                            if (v > 0.f)
                                scale = v;
                        }
                        else if (field == U"color")
                        {
                            color = _parse_hex_color(value);
                        }
                        else if (field == U"offset")
                        {
                            float dx = 0.f, dy = 0.f;
                            if (2 == std::sscanf(
                                    _u32_to_utf8(
                                        value.c_str(), value.size()).c_str(),
                                    "(%f,%f)", &dx, &dy))
                            {
                                offx = dx;
                                offy = dy;
                            }
                        }
                        // 未知标记：忽略（与旧实现一致）
                        i = j + 1;
                    }
                    else
                    {
                        i = j; // 未闭合：消费至结尾
                    }
                    continue;
                }

                emit_char(ch);
                ++i;
            }
        }
    };
}
