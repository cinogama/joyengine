#pragma once
#include <cstdint>
#include <cstddef>

// Main cache version
constexpr uint32_t CACHE_MANAGER_MAIN_VERSION = 0x00000003;

// Sub cache versions
constexpr uint32_t SHADER_CACHE_VERSION = 0x00000290;
// Image texture property cache (color space); payload is a uint32_t.
constexpr uint32_t IMAGE_TEXTURE_CACHE_VERSION = 0x00000001;
// Frame animation cache generated from the .je4animation TOML source; the
// payload is a binary cache document (see jeecs_cache_binary_* in jeecs.hpp).
constexpr uint32_t ANIMATION_CACHE_VERSION = 0x00000001;
