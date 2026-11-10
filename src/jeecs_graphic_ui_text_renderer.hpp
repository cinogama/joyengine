#pragma once

// UI 文本逐字渲染器（引擎内部实现，不属于公共 C-API）。
//
// 与旧的 text_texture_impl（整段文本 CPU 逐像素合成一张纹理）的本质区别：
// 文本永不构成纹理。每个可见字符都是独立四边形面片，逐字携带位置 / UV /
// 颜色，批量写入共享动态顶点缓冲后按连续段绘制（见
// UserInterfaceGraphicPipelineSystem 的接入代码）。
//
// 核心不变量：
//  * 字形按（字体, 量化字号, 码点）经 je_font_char_get 光栅化一次（复用其
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
#   define JE_IMPL
#endif
#include "jeecs/basic.hpp"
#include "jeecs/components.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <unordered_map>
#include <vector>

namespace jeecs::graphic::ui
{
    class TextRenderer final
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
            basic::resource<graphic::texture2d> m_texture;
            size_t m_cursor_x;
            size_t m_cursor_y;
            size_t m_row_height;
            uint64_t m_last_used_frame;
            // 页纹理最近一次已知的图形线程版本：版本变化（reboot）后
            // 页内容被重建，需要丢弃整个图集缓存重新光栅化
            je_version_t m_graphic_thread_version;

            explicit page_t(basic::resource<graphic::texture2d>&& tex) noexcept
                : m_texture(std::move(tex))
                , m_cursor_x(PAGE_GUTTER)
                , m_cursor_y(PAGE_GUTTER)
                , m_row_height(0)
                , m_last_used_frame(0)
                , m_graphic_thread_version(0)
            {
            }
        };

        // 图集中的一个字形：UV（含半像素内缩）+ 布局度量。布局（量宽）与
        // 发射（取 UV）共用；无位图的字形（空格等，w/h 为 0）只保留度量。
        struct glyph_slot
        {
            float u0, v0, u1, v1;
            int16_t w, h;
            int16_t baseline_offset_x, baseline_offset_y;
            float advance_x;
            page_t* page; // 所属图集页；无位图为 nullptr

            glyph_slot()
                : u0(0.f)
                , v0(0.f)
                , u1(0.f)
                , v1(0.f)
                , w(0)
                , h(0)
                , baseline_offset_x(0)
                , baseline_offset_y(0)
                , advance_x(0.f)
                , page(nullptr)
            {
            }
        };

        // 行网格度量：仅取决于（字体, 基准字号），与文本内容无关。
        struct font_metrics_t
        {
            float line_height; // 行距：基线到基线
            float ascent;      // 基线以上部分（像素）
            float descent;     // 基线以下部分（像素，正值）

            font_metrics_t()
                : line_height(0.f)
                , ascent(0.f)
                , descent(0.f)
            {
            }
        };

        // 每帧首调用：推进帧计数并做 LRU 淘汰。
        void begin_frame() noexcept;

        // 解析（必要时光栅化并装箱）一个字形。同帧内返回的指针稳定；
        // 跨帧不保证（页可能被淘汰），发射端应每帧重新解析。
        // update_cmd 是资源更新命令缓冲：新字形装箱时向其录制图集区段上传。
        const glyph_slot* resolve(
            const basic::resource<graphic::font>& font,
            int raster_px, char32_t cp,
            je_gl_command_buffer_t* update_cmd);

        // 行网格度量。行距取自任意字形的 m_advance_y（同一字号下全字符
        // 一致），像素换算比例由 行距/字体行高单位 推出，ascent/descent
        // 随之确定——全程无需新增字体 C-API。
        static font_metrics_t font_metrics(graphic::font& f, int font_px) noexcept;

        // 文本块左下角在矩形局部系（原点=矩形中心）中的位置：
        // 按对齐位掩码做九宫格放置（块不拉伸，溢出对称外扩）。
        static math::vec2 block_origin_local(
            const UserInterface::Element::resolved_rect& rect,
            const UserInterface::Text::runtime_layout_t& layout,
            UserInterface::Element::alignment align,
            float camera_scale) noexcept;

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
            float win_w, float win_h,
            je_gl_command_buffer_t* update_cmd);

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
            uint64_t m_last_used_frame;

            font_atlas_t()
                : m_last_used_frame(0)
            {
            }
        };

        std::unordered_map<je_font_t*, font_atlas_t> m_fonts;
        size_t m_total_pages;
        uint64_t m_frame;
        bool m_warned_page_eviction;

    public:
        TextRenderer()
            : m_total_pages(0)
            , m_frame(0)
            , m_warned_page_eviction(false)
        {
        }

    private:

        glyph_slot& _create_glyph(
            font_atlas_t& entry, int raster_px, char32_t cp, uint64_t key,
            je_gl_command_buffer_t* update_cmd);
        void _pack_glyph(
            font_atlas_t& entry, glyph_slot& slot, const graphic::character& src,
            je_gl_command_buffer_t* update_cmd);
        void _blit_glyph(
            page_t& page, glyph_slot& slot, const graphic::character& src,
            je_gl_command_buffer_t* update_cmd);
        void _new_page(font_atlas_t& entry);
        bool _evict_one_lru_page();
        void _drop_all_pages_and_glyphs() noexcept;

        // ---- 标记解析 ----
        static bool _is_space(char32_t c) noexcept;
        static bool _is_breakable(char32_t c) noexcept;
        static std::u32string _utf8_to_u32(const std::string& s);
        static std::string _u32_to_utf8(const char32_t* s, size_t n);
        static math::vec4 _parse_hex_color(const std::u32string& hex);
        static void _parse_markup(
            const std::string& content, std::vector<styled_char_t>& out);
    };
}
