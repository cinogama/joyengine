#define JE_IMPL
#include "jeecs.hpp"

#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "stb_truetype.h"

struct je_stb_font_data
{
    stbtt_fontinfo  m_font;
    uint8_t* m_font_file_buf;

    float           m_scale_x;
    float           m_scale_y;

    size_t          m_board_size_x;
    size_t          m_board_size_y;

    int32_t         m_ascent;
    int32_t         m_descent;
    int32_t         m_line_gap;
    int32_t         m_line_space;

    float           m_x_scale_for_pix;
    float           m_y_scale_for_pix;

    je_font_char_updater_t m_updater;

    std::mutex                                         m_character_set_mx;
    std::map<unsigned long, jeecs::graphic::character> m_character_set;
};

je_font* je_font_load(
    const char* font_path,
    float                   scalex,
    float                   scaley,
    size_t                  board_blank_size_x,
    size_t                  board_blank_size_y,
    je_font_char_updater_t  char_texture_updater)
{
    je_stb_font_data* fontdata = new je_stb_font_data;
    fontdata->m_font_file_buf = nullptr;

    if (auto* file = jeecs_file_open(font_path))
    {
        fontdata->m_font_file_buf = (uint8_t*)malloc(file->m_file_length);
        jeecs_file_read(
            fontdata->m_font_file_buf,
            sizeof(uint8_t),
            file->m_file_length,
            file);

        jeecs_file_close(file);

        fontdata->m_scale_x = scalex;
        fontdata->m_scale_y = scaley;
        fontdata->m_board_size_x = board_blank_size_x;
        fontdata->m_board_size_y = board_blank_size_y;
        fontdata->m_updater = char_texture_updater;

        stbtt_InitFont(&fontdata->m_font, fontdata->m_font_file_buf,
            stbtt_GetFontOffsetForIndex(fontdata->m_font_file_buf, 0));

        int ascent, descent, line_gap;
        stbtt_GetFontVMetrics(&fontdata->m_font, &ascent, &descent, &line_gap);

        fontdata->m_ascent = (int32_t)ascent;
        fontdata->m_descent = (int32_t)descent;
        fontdata->m_line_gap = (int32_t)line_gap;

        // https://www.ffutop.com/posts/2024-06-19-freetype-glyph/
        fontdata->m_line_space = fontdata->m_ascent - fontdata->m_descent + fontdata->m_line_gap;

        fontdata->m_x_scale_for_pix =
            stbtt_ScaleForPixelHeight(&fontdata->m_font, fontdata->m_scale_x);
        fontdata->m_y_scale_for_pix =
            stbtt_ScaleForPixelHeight(&fontdata->m_font, fontdata->m_scale_y);
    }
    else
    {
        assert(fontdata->m_font_file_buf == nullptr);
        delete fontdata;
        fontdata = nullptr;
    }

    return fontdata;
}

void je_font_free(je_font* font)
{
    if (font)
    {
        assert(font->m_font_file_buf);
        free(font->m_font_file_buf);
        delete font;
    }
}

jeecs::graphic::character* je_font_get_char(je_font* font, unsigned long chcode)
{
    assert(font != nullptr);

    std::lock_guard g1(font->m_character_set_mx);

    if (auto fnd = font->m_character_set.find(chcode); fnd != font->m_character_set.end())
        return &fnd->second;

    jeecs::graphic::character& ch = font->m_character_set[chcode];

    ch.m_char = (wchar_t)chcode;

    /////////////////////////////////////////////////
    int x0, y0, x1, y1, advance, lsb, pixel_w, pixel_h;

    stbtt_GetCodepointHMetrics(&font->m_font, chcode, &advance, &lsb);
    unsigned char* ch_tex_buffer = nullptr;

    if (stbtt_GetCodepointBox(&font->m_font, chcode, &x0, &y0, &x1, &y1))
    {
        ch_tex_buffer = stbtt_GetCodepointBitmap(
            &font->m_font,
            font->m_x_scale_for_pix,
            font->m_y_scale_for_pix,
            chcode,
            &pixel_w,
            &pixel_h,
            nullptr,
            nullptr);
    }

    if (ch_tex_buffer == nullptr)
    {
        // 无法生成字符纹理
        pixel_w = 0;
        pixel_h = 0;
    }

    // 由于边框对字形大小没有发生影响，只是外围扩大了一圈
    // 所以为了保证所有显示仍然正确，需要让字体的大小扩大，
    // 基线偏移亦要考虑边框
    ch.m_width = pixel_w + 2 * (int)font->m_board_size_x;
    ch.m_height = pixel_h + 2 * (int)font->m_board_size_y;
    ch.m_advance_x = (int)round(font->m_x_scale_for_pix * (float)advance);
    ch.m_advance_y = -(int)round(font->m_y_scale_for_pix * (float)font->m_line_space);
    ch.m_baseline_offset_x = pixel_w ? (int)round(font->m_x_scale_for_pix * (float)x0) - (int)font->m_board_size_x : 0;
    ch.m_baseline_offset_y = pixel_h ? (int)round(font->m_y_scale_for_pix * (float)y0) - (int)font->m_board_size_y : 0;

    ch.m_texture =
        jeecs::graphic::texture::create(
            (size_t)ch.m_width,
            (size_t)ch.m_height,
            jegl_texture::format::RGBA);

    if (ch_tex_buffer != nullptr)
    {
        for (size_t j = 0; j < (size_t)pixel_h; j++)
        {
            for (size_t i = 0; i < (size_t)pixel_w; i++)
            {
                float _vl = ((float)ch_tex_buffer[i + pixel_w * j]) / 255.0f;
                ch.m_texture->pix(i + font->m_board_size_x, pixel_h - j - 1 + font->m_board_size_y).set(
                    { 1.f, 1.f, 1.f, _vl }
                );
            }
        }
        free(ch_tex_buffer);
    }

    if (font->m_updater != nullptr)
    {
        auto* raw_texture_data = ch.m_texture->resource()->m_raw_texture_data;
        font->m_updater(
            raw_texture_data->m_pixels,
            raw_texture_data->m_width,
            raw_texture_data->m_height);
    }

    return &ch;
}

