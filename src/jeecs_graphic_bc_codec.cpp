#define JE_IMPL
#include "jeecs/graphic.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs_graphic_render_internal.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>

// BC1/BC3 (DXT1/DXT5) 编码使用 3rd/stb 的 stb_dxt（公有领域）。只用它完成
// 单个 4x4 块的压缩；格式选择、块遍历与解码在本文件内实现。
#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

namespace jeecs::graphic
{
    namespace
    {
        // 每个 4x4 块的字节数；非块压缩格式返回 0。
        size_t _bc_block_bytes(je_gl_texture_format_t format)
        {
            switch (format & JE_GL_TEXTURE_FORMAT_COMPRESSED_MASK)
            {
            case JE_GL_TEXTURE_FORMAT_BC1:
                return 8;
            case JE_GL_TEXTURE_FORMAT_BC3:
                return 16;
            default:
                return 0;
            }
        }

        inline uint8_t _expand_5bit_to_8bit(uint8_t value)
        {
            return (uint8_t)((value << 3) | (value >> 2));
        }
        inline uint8_t _expand_6bit_to_8bit(uint8_t value)
        {
            return (uint8_t)((value << 2) | (value >> 4));
        }

        // 解码 BC1 颜色块（8 字节）到 16 个 RGBA 像素。
        // bc3_color_block 为 true 时按 BC3 的颜色块规则处理：恒为 4 色插值，
        // 不使用 BC1 的三色 + 透明模式。
        void _decode_bc_color_block(
            const uint8_t* block,
            bool bc3_color_block,
            je_gl_texture_pixel_data_t* out_pixels)
        {
            const uint16_t c0 = (uint16_t)(block[0] | (block[1] << 8));
            const uint16_t c1 = (uint16_t)(block[2] | (block[3] << 8));

            uint8_t colors[4][3];
            colors[0][0] = _expand_5bit_to_8bit((uint8_t)((c0 >> 11) & 0x1F));
            colors[0][1] = _expand_6bit_to_8bit((uint8_t)((c0 >> 5) & 0x3F));
            colors[0][2] = _expand_5bit_to_8bit((uint8_t)(c0 & 0x1F));

            colors[1][0] = _expand_5bit_to_8bit((uint8_t)((c1 >> 11) & 0x1F));
            colors[1][1] = _expand_6bit_to_8bit((uint8_t)((c1 >> 5) & 0x3F));
            colors[1][2] = _expand_5bit_to_8bit((uint8_t)(c1 & 0x1F));

            bool has_transparent_index = false;
            if (bc3_color_block || c0 > c1)
            {
                for (size_t i = 0; i < 3; ++i)
                {
                    colors[2][i] = (uint8_t)((2 * colors[0][i] + colors[1][i]) / 3);
                    colors[3][i] = (uint8_t)((colors[0][i] + 2 * colors[1][i]) / 3);
                }
            }
            else
            {
                for (size_t i = 0; i < 3; ++i)
                    colors[2][i] = (uint8_t)((colors[0][i] + colors[1][i]) / 2);
                colors[3][0] = colors[3][1] = colors[3][2] = 0;
                has_transparent_index = true;
            }

            const uint32_t indices =
                (uint32_t)block[4] | ((uint32_t)block[5] << 8)
                | ((uint32_t)block[6] << 16) | ((uint32_t)block[7] << 24);

            for (size_t i = 0; i < 16; ++i)
            {
                const size_t color_index = (indices >> (2 * i)) & 0x3;
                je_gl_texture_pixel_data_t* pixel = out_pixels + i * 4;

                pixel[0] = colors[color_index][0];
                pixel[1] = colors[color_index][1];
                pixel[2] = colors[color_index][2];
                pixel[3] = (has_transparent_index && color_index == 3) ? 0 : 255;
            }
        }

        // 解码 BC3 的 alpha 块（8 字节，48 位 3 位索引）到 16 个 alpha。
        void _decode_bc3_alpha_block(
            const uint8_t* block,
            uint8_t* out_alphas)
        {
            const uint8_t a0 = block[0];
            const uint8_t a1 = block[1];

            uint8_t alphas[8];
            alphas[0] = a0;
            alphas[1] = a1;

            if (a0 > a1)
            {
                for (size_t i = 0; i < 6; ++i)
                {
                    alphas[2 + i] = (uint8_t)(
                        ((6 - i) * a0 + (1 + i) * a1) / 7);
                }
            }
            else
            {
                for (size_t i = 0; i < 4; ++i)
                {
                    alphas[2 + i] = (uint8_t)(
                        ((4 - i) * a0 + (1 + i) * a1) / 5);
                }
                alphas[6] = 0;
                alphas[7] = 255;
            }

            uint64_t indices = 0;
            for (size_t i = 0; i < 6; ++i)
                indices |= (uint64_t)block[2 + i] << (8 * i);

            for (size_t i = 0; i < 16; ++i)
                out_alphas[i] = alphas[(indices >> (3 * i)) & 0x7];
        }
    }

    size_t texture_format_row_bytes(
        je_gl_texture_format_t format, size_t width)
    {
        const size_t block_bytes = _bc_block_bytes(format);
        if (block_bytes != 0)
            return ((width + 3) / 4) * block_bytes;

        size_t pixel_size =
            (size_t)(format & JE_GL_TEXTURE_FORMAT_COLOR_DEPTH_MASK);
        assert(pixel_size != 0);

        if (0 != (format & JE_GL_TEXTURE_FORMAT_FLOAT16))
            pixel_size *= 2;

        return width * pixel_size;
    }

    size_t texture_format_pixel_data_size(
        je_gl_texture_format_t format, size_t width, size_t height)
    {
        if (0 != (format & JE_GL_TEXTURE_FORMAT_COMPRESSED_MASK))
        {
            // 块压缩格式的一"行"是 4 像素高的块行。
            return texture_format_row_bytes(format, width)
                * ((height + 3) / 4);
        }
        return texture_format_row_bytes(format, width) * height;
    }

    je_gl_texture_format_t texture_bc_format_for_rgba(
        size_t width, size_t height,
        const je_gl_texture_pixel_data_t* rgba_pixels)
    {
        const size_t pixel_count = width * height;
        for (size_t i = 0; i < pixel_count; ++i)
        {
            if (rgba_pixels[i * 4 + 3] != 255)
                return JE_GL_TEXTURE_FORMAT_BC3;
        }

        // 全部不透明：BC1 无损表达 alpha（恒为不透明），体积只有 BC3 的一半。
        return JE_GL_TEXTURE_FORMAT_BC1;
    }

    /* OPTIONAL */ je_gl_texture_pixel_data_t* compress_texture_bc(
        je_gl_texture_format_t format, size_t width, size_t height,
        const je_gl_texture_pixel_data_t* rgba_pixels)
    {
        const size_t block_bytes = _bc_block_bytes(format);
        assert(block_bytes != 0);
        assert(width != 0 && height != 0);

        const size_t blocks_x = (width + 3) / 4;
        const size_t blocks_y = (height + 3) / 4;
        const size_t block_count = blocks_x * blocks_y;

        auto* const block_data = (je_gl_texture_pixel_data_t*)malloc(
            block_count * block_bytes);
        if (block_data == nullptr)
            return nullptr;

        // stb_dxt 会按 4 字节整数读取块数据，这里保证对齐。
        alignas(4) je_gl_texture_pixel_data_t block_pixels[16 * 4];
        je_gl_texture_pixel_data_t* dst = block_data;

        for (size_t by = 0; by < blocks_y; ++by)
        {
            for (size_t bx = 0; bx < blocks_x; ++bx)
            {
                // 边缘块用钳制后的像素填充（复制最后一行/列），采样时
                // 越界部分不会被读取。
                for (size_t y = 0; y < 4; ++y)
                {
                    const size_t src_y = std::min(by * 4 + y, height - 1);
                    for (size_t x = 0; x < 4; ++x)
                    {
                        const size_t src_x = std::min(bx * 4 + x, width - 1);
                        memcpy(
                            block_pixels + (y * 4 + x) * 4,
                            rgba_pixels + (src_y * width + src_x) * 4,
                            4);
                    }
                }

                // alpha 参数：BC3 需要写入 8 位 alpha 块；BC1 按不透明处理
                //（格式选择已保证 BC1 只用于全不透明图片）。
                stb_compress_dxt_block(
                    dst, block_pixels,
                    block_bytes == 16 ? 1 : 0,
                    STB_DXT_HIGHQUAL);

                dst += block_bytes;
            }
        }

        return block_data;
    }

    /* OPTIONAL */ je_gl_texture_pixel_data_t* decode_texture_bc(
        je_gl_texture_format_t format, size_t width, size_t height,
        const je_gl_texture_pixel_data_t* block_data)
    {
        const size_t block_bytes = _bc_block_bytes(format);
        assert(block_bytes != 0);
        assert(width != 0 && height != 0);

        const size_t blocks_x = (width + 3) / 4;
        const size_t blocks_y = (height + 3) / 4;

        auto* const pixels = (je_gl_texture_pixel_data_t*)malloc(
            width * height * 4);
        if (pixels == nullptr)
            return nullptr;

        uint8_t block_alphas[16];
        je_gl_texture_pixel_data_t block_pixels[16 * 4];

        for (size_t by = 0; by < blocks_y; ++by)
        {
            for (size_t bx = 0; bx < blocks_x; ++bx)
            {
                const je_gl_texture_pixel_data_t* const block =
                    block_data + (by * blocks_x + bx) * block_bytes;

                if (block_bytes == 16)
                {
                    _decode_bc3_alpha_block(block, block_alphas);
                    _decode_bc_color_block(block + 8, true, block_pixels);
                }
                else
                {
                    memset(block_alphas, 255, sizeof(block_alphas));
                    _decode_bc_color_block(block, false, block_pixels);
                }

                for (size_t y = 0; y < 4; ++y)
                {
                    const size_t dst_y = by * 4 + y;
                    if (dst_y >= height)
                        break;

                    for (size_t x = 0; x < 4; ++x)
                    {
                        const size_t dst_x = bx * 4 + x;
                        if (dst_x >= width)
                            break;

                        const size_t pixel_index = y * 4 + x;
                        je_gl_texture_pixel_data_t* const dst_pixel =
                            pixels + (dst_y * width + dst_x) * 4;

                        dst_pixel[0] = block_pixels[pixel_index * 4 + 0];
                        dst_pixel[1] = block_pixels[pixel_index * 4 + 1];
                        dst_pixel[2] = block_pixels[pixel_index * 4 + 2];
                        // BC3 的 alpha 来自独立的 8 位 alpha 块；BC1 的 alpha
                        // 由颜色块给出（全部不透明，或三色模式下的透明索引）。
                        dst_pixel[3] = block_bytes == 16
                            ? block_alphas[pixel_index]
                            : block_pixels[pixel_index * 4 + 3];
                    }
                }
            }
        }

        return pixels;
    }
}
