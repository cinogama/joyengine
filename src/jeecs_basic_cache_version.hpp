#pragma once
#include <cstdint>
#include <cstddef>

// Main cache version
constexpr uint32_t CACHE_MANAGER_MAIN_VERSION = 0x00000003;

// Sub cache versions
constexpr uint32_t SHADER_CACHE_VERSION = 0x00000290;
// Image texture cache generated from the image file and its optional
// .je4texture TOML source; the payload is a binary cache document
// (width/height/color_space -> INT32, pixels -> RGBA8 BUFFER), keyed by
// crc64(image file bytes chained with .je4texture bytes).
constexpr uint32_t IMAGE_TEXTURE_CACHE_VERSION = 0x00000003;
// Animation set cache generated from the .je4animation TOML source (v2
// track/key model); the payload is a binary cache document
// (see jeecs_cache_binary_* in jeecs.hpp and jeecs_graphic_animation_cache.cpp).
constexpr uint32_t ANIMATION_CACHE_VERSION = 0x00000002;
// Tile atlas/tilemap caches generated from the .je4tileset/.je4tilemap TOML
// sources; payloads are binary cache documents.
constexpr uint32_t TILESET_CACHE_VERSION = 0x00000001;
constexpr uint32_t TILEMAP_CACHE_VERSION = 0x00000001;

constexpr uint32_t INTERNAL_SCRIPT_CACHE_VERSION = 0x00000001;

constexpr uint32_t EDITOR_SCRIPT_CACHE_VERSION = 0x00000001;
