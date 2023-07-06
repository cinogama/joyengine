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

    jegl_texture::sampling m_sampling;

    // NOTE: Multi-thread unsfe!
    std::map<unsigned long, jeecs::graphic::character> character_set;
};

je_font* je_font_load(const char* fontPath, float scalex, float scaley, jegl_texture::sampling samp)
{
    je_stb_font_data* fontdata = new je_stb_font_data;
    fontdata->m_font_file_buf = nullptr;
    fontdata->m_sampling = samp;

    if (auto* file = jeecs_file_open(fontPath))
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

        stbtt_InitFont(&fontdata->m_font, fontdata->m_font_file_buf, stbtt_GetFontOffsetForIndex(fontdata->m_font_file_buf, 0));
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

    if (auto fnd = font->character_set.find(chcode); fnd != font->character_set.end())
        return &fnd->second;

    jeecs::graphic::character& ch = font->character_set[chcode];

    ch.m_char = (wchar_t)chcode;

    /////////////////////////////////////////////////

    auto real_scalex = stbtt_ScaleForPixelHeight(&font->m_font, font->m_scale_x);
    auto real_scaley = stbtt_ScaleForPixelHeight(&font->m_font, font->m_scale_y);

    int x0, y0, x1, y1, advance, lsb, pixel_w, pixel_h;

    stbtt_GetCodepointHMetrics(&font->m_font, chcode, &advance, &lsb);
    stbtt_GetCodepointBitmapBoxSubpixel(&font->m_font, chcode, real_scalex, real_scaley, 0, 0, &x0, &y0, &x1, &y1);

    auto ch_tex_buffer = stbtt_GetCodepointBitmap(
        &font->m_font,
        real_scalex,
        real_scaley,
        chcode,
        &pixel_w,
        &pixel_h,
        nullptr,
        nullptr);

    ch.m_width = x1 - x0;
    ch.m_height = y1 - y0;
    ch.m_advised_w = (int)(real_scalex * (float)advance);
    ch.m_advised_h = (int)font->m_scale_y;
    ch.m_baseline_offset_x = x0;
    ch.m_baseline_offset_y = y0;

    jeecs::graphic::texture* tex =
        jeecs::graphic::texture::create(pixel_w, pixel_h, jegl_texture::format::RGBA, jegl_texture::sampling::DEFAULT);
    tex->resouce()->m_raw_texture_data->m_sampling = font->m_sampling;

    for (int j = 0; j < pixel_h; j++)
    {
        for (int i = 0; i < pixel_w; i++)
        {
            float _vl = ((float)ch_tex_buffer[i + pixel_w * j]) / 255.0f;
            tex->pix(i, pixel_h - j - 1).set(
                { 1.f, 1.f, 1.f, _vl }
            );
        }
    }

    free(ch_tex_buffer);
    ch.m_texture = tex;

    return &ch;
}

