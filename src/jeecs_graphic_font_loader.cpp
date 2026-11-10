#define JE_IMPL
#include "jeecs/file.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/graphic.hpp"

#define STB_TRUETYPE_IMPLEMENTATION // force following include to generate implementation
#include "stb_truetype.h"

using namespace std;
using namespace jeecs;
using namespace jeecs::graphic;

struct je_stb_font_data_t
{
    stbtt_fontinfo m_font;
    mutex m_character_set_mx;
    // 字形缓存按（字号, 码点）两级索引；字号在 je_font_char_get 调用时给出
    map<float, map<char32_t, character>> m_character_set;
};

/* OPTIONAL */ je_font_t* je_font_load(
    const char* font_path,
    size_t board_blank_size_x,
    size_t board_blank_size_y,
    /* OPTIONAL */ je_font_char_updater_t char_texture_updater)
{
    je_font_t* font = new je_font_t;
    font->m_stb_font_data = new je_stb_font_data_t;

    font->m_path = strdup(font_path);
    font->m_font_file_buf = nullptr;

    bool font_initialized = false;

    if (auto* file = je_file_open(font_path))
    {
        font->m_font_file_buf = (uint8_t*)malloc(je_file_size(file));
        je_file_read(
            font->m_font_file_buf,
            sizeof(uint8_t),
            je_file_size(file),
            file);

        je_file_close(file);

        font->m_board_size_x = board_blank_size_x;
        font->m_board_size_y = board_blank_size_y;
        font->m_updater = char_texture_updater;

        font_initialized =
            stbtt_InitFont(&font->m_stb_font_data->m_font, font->m_font_file_buf,
                stbtt_GetFontOffsetForIndex(font->m_font_file_buf, 0)) != 0;

        if (font_initialized)
        {
            int ascent, descent, line_gap;
            stbtt_GetFontVMetrics(&font->m_stb_font_data->m_font, &ascent, &descent, &line_gap);

            font->m_ascent = (int32_t)ascent;
            font->m_descent = (int32_t)descent;
            font->m_line_gap = (int32_t)line_gap;

            // https://www.ffutop.com/posts/2024-06-19-freetype-glyph/
            font->m_line_space = font->m_ascent - font->m_descent + font->m_line_gap;
        }
    }

    if (!font_initialized)
    {
        free(font->m_font_file_buf);

        free(const_cast<char*>(font->m_path));

        delete font->m_stb_font_data;
        delete font;
        font = nullptr;
    }

    return font;
}

void je_font_free(je_font_t* font)
{
    assert(font != nullptr);

    assert(font->m_font_file_buf);
    free(font->m_font_file_buf);
    free(const_cast<char*>(font->m_path));

    delete font->m_stb_font_data;
    delete font;
}

const character* je_font_char_get(
    je_font_t* font, float size, char32_t unicode32_char)
{
    assert(font != nullptr);
    assert(size > 0.f);

    lock_guard g1(font->m_stb_font_data->m_character_set_mx);

    auto& sized_char_set = font->m_stb_font_data->m_character_set[size];

    if (auto fnd = sized_char_set.find(unicode32_char);
        fnd != sized_char_set.end())
        return &fnd->second;

    /////////////////////////////////////////////////
    int x0, y0, x1, y1, advance, lsb, pixel_w, pixel_h;

    const float scale =
        stbtt_ScaleForPixelHeight(&font->m_stb_font_data->m_font, size);

    stbtt_GetCodepointHMetrics(
        &font->m_stb_font_data->m_font,
        static_cast<int>(unicode32_char),
        &advance,
        &lsb);

    unsigned char* ch_tex_buffer = nullptr;

    // 位图相对基线的像素空间偏移，由光栅化结果直接给出：
    // bmp_x0 是墨水左列；bmp_y0 是墨水顶边（stb 的 y 轴向下，负值表示墨水顶在基线上方）。
    // 不能用字体单位包围盒乘 scale 再舍入来推算，否则舍入方向与光栅化不一致，
    // 带 overshoot 的圆形字母（o/a 等）会相对平底字母（n 等）整体偏移 1px。
    int bmp_x0 = 0, bmp_y0 = 0;

    if (stbtt_GetCodepointBox(
        &font->m_stb_font_data->m_font,
        static_cast<int>(unicode32_char),
        &x0,
        &y0,
        &x1,
        &y1))
    {
        ch_tex_buffer = stbtt_GetCodepointBitmap(
            &font->m_stb_font_data->m_font,
            scale,
            scale,
            static_cast<int>(unicode32_char),
            &pixel_w,
            &pixel_h,
            &bmp_x0,
            &bmp_y0);
    }

    if (ch_tex_buffer == nullptr)
    {
        // 无法生成字符纹理
        pixel_w = 0;
        pixel_h = 0;
    }

    int texture_pixel_width = pixel_w + 2 * (int)font->m_board_size_x;
    int texture_pixel_height = pixel_h + 2 * (int)font->m_board_size_y;

    // 字形纹理内容在创建时一次确定：先在临时缓冲合成（白色 + 覆盖度
    // alpha，含 board 边框），经 updater 预处理后作为初始像素创建纹理
    const size_t texture_pixel_count =
        (size_t)texture_pixel_width * (size_t)texture_pixel_height * 4;
    std::vector<uint8_t> glyph_pixels(texture_pixel_count, 0);

    if (ch_tex_buffer != nullptr)
    {
        for (size_t j = 0; j < (size_t)pixel_h; j++)
        {
            for (size_t i = 0; i < (size_t)pixel_w; i++)
            {
                const uint8_t coverage = ch_tex_buffer[i + pixel_w * j];
                uint8_t* dst = glyph_pixels.data()
                    + (((size_t)pixel_h - j - 1 + font->m_board_size_y)
                        * (size_t)texture_pixel_width
                        + (i + font->m_board_size_x)) * 4;
                dst[0] = dst[1] = dst[2] = 0xFF;
                dst[3] = coverage;
            }
        }
        free(ch_tex_buffer);
    }

    if (font->m_updater != nullptr)
    {
        font->m_updater(
            glyph_pixels.data(),
            (size_t)texture_pixel_width,
            (size_t)texture_pixel_height);
    }

    character& ch =
        sized_char_set.emplace(
                unicode32_char,
                character{
                    texture2d::create(
                        (size_t)texture_pixel_width,
                        (size_t)texture_pixel_height,
                        JE_GL_TEXTURE_FORMAT_RGBA,
                        glyph_pixels.data()) }).first->second;

    ch.m_char = unicode32_char;

    // 由于边框对字形大小没有发生影响，只是外围扩大了一圈
    // 所以为了保证所有显示仍然正确，需要让字体的大小扩大，
    // 基线偏移亦要考虑边框
    ch.m_width = texture_pixel_width;
    ch.m_height = texture_pixel_height;
    ch.m_advance_x = (int)round(scale * (float)advance);
    ch.m_advance_y = -(int)round(scale * (float)font->m_line_space);
    ch.m_baseline_offset_x = pixel_w ? bmp_x0 - (int)font->m_board_size_x : 0;
    // 字符纹理的 y 轴向上、第 0 行是墨水底边下方 board 处，而 bmp_y0 以 y 向下
    // 记录墨水顶边，故墨水底边在基线上方 -(bmp_y0 + pixel_h) 像素
    ch.m_baseline_offset_y = pixel_h ? -(bmp_y0 + pixel_h) - (int)font->m_board_size_y : 0;

    return &ch;
}
