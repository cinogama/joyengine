#pragma once
#include <cstdint>
#include <cstddef>

// Main cache version
constexpr uint32_t CACHE_MANAGER_MAIN_VERSION = 0x00000003;

// Sub cache versions
constexpr uint32_t SHADER_CACHE_VERSION = 0x00000290;
// Image texture cache generated from the image file and its optional
// .je4texture TOML source; the payload is a binary cache document
// (width/height/color_space/format -> INT32, pixels -> BUFFER), keyed by
// crc64(image file bytes chained with .je4texture bytes). v4 stores the
// pixels block-compressed (BC1/BC3, "format" = 1/2) by default; "format" = 0
// means raw RGBA8, as written by v3.
constexpr uint32_t IMAGE_TEXTURE_CACHE_VERSION = 0x00000004;
// Animation set cache generated from the .je4animation TOML source (v2
// track/key model); the payload is a binary cache document
// (see jeecs_cache_binary_* in jeecs.hpp and jeecs_graphic_animation_cache.cpp).
constexpr uint32_t ANIMATION_CACHE_VERSION = 0x00000002;
// Tile atlas/tilemap caches generated from the .je4tileset/.je4tilemap TOML
// sources; payloads are binary cache documents. Tileset v2 adds the built-in
// audio occlusion/muffle coefficients to each tile entry.
constexpr uint32_t TILESET_CACHE_VERSION = 0x00000002;
constexpr uint32_t TILEMAP_CACHE_VERSION = 0x00000001;
// Audio cache generated from the audio file and its optional .je4audio TOML
// source; the payload is a raw interleaved PCM region followed by a binary
// cache document:
//   [uint64 pcm_size][pcm bytes]                      (raw, no value tag)
//   DOCUMENT channels/samplerate/bits -> INT32,
//            is_float/streaming -> BOOL
// The PCM is stored in the .je4audio "format", converted by pkg::audio,
// defaulting to 8-bit integer. The raw region can be accessed by offset
// without parsing the document, which streaming playback will use. Keyed by
// crc64(audio file bytes chained with .je4audio bytes). v2: the pcm is always
// stored converted to the configured cache format instead of the raw source
// encoding. v3: the pcm moved out of the document into a raw leading region.
constexpr uint32_t AUDIO_CACHE_VERSION = 0x00000003;

constexpr uint32_t INTERNAL_SCRIPT_CACHE_VERSION = 0x00000001;

constexpr uint32_t EDITOR_SCRIPT_CACHE_VERSION = 0x00000001;
