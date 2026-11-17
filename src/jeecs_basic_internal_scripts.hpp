#pragma once

// ============================================================================
// jeecs_basic_internal_scripts.hpp —— 内置缓存生成器的重建入口
//
// 各资产域的 open_or_create_*_cache 归属各自模块（动画集在
// jeecs::animation、图片纹理在 jeecs::graphic、音频在 jeecs::audio、
// 瓦片地图/瓦片集在 jeecs::Tilemap）；本头只提供"用
// builtin/script/internal/cache/<asset>.wo 生成器重建缓存"这一手段，
// 实现位于 jeecs_basic_internal_scripts.cpp（生成器环境由
// internal_script 模块持有），不对游戏项目开放。
// ============================================================================

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/file.hpp"

#include <cstdint>

namespace jeecs::internal_script
{
    // 成功时返回重新加载的只读缓存句柄（调用方负责关闭）；
    // 生成器缺失或生成失败返回 nullptr。
    // crc64_key 语义同 je_cache_file_create：0 由路径字节推导，
    // 其余值为键本身（须与打开侧 je_cache_file_load 共用同一键）。

    /* OPTIONAL */ je_file_t* regenerate_animation_cache(const char* path);
    /* OPTIONAL */ je_file_t* regenerate_tilemap_cache(const char* path);
    /* OPTIONAL */ je_file_t* regenerate_tileset_cache(const char* path);
    /* OPTIONAL */ je_file_t* regenerate_image_texture_cache(
        const char* path, uint64_t crc64_key);
    /* OPTIONAL */ je_file_t* regenerate_audio_cache(
        const char* path, uint64_t crc64_key);
}
