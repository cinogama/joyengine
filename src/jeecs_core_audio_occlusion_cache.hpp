#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif

// ============================================================================
// jeecs_core_audio_occlusion_cache.hpp —— 静态遮挡烘焙/缓存与 2D 遮挡场
//
// 世界缓存 <world>.je4cache 的 "audio_occlusion" 字段保存世界空间 2D 遮挡场：
//   * 输入：is_static 的 Audio::Occluder（世界变换已解析）+ 全部 Tilemap::Map
//     的音频遮挡（瓦片地图视为静态）；
//   * 每个格存“单位长度光学密度”(-ln(透过率) / 世界单位)，运行时沿射线
//     积分密度得到与厚度/入射角相关的平滑遮挡；
//   * 动态遮挡体（is_static = false）不进入缓存，运行时逐帧线段测试。
//
// 场只服务 2D（Listener::use_2d = true）；3D 仍走逐遮挡体的实时路径。
// ============================================================================

#include "jeecs/audio.hpp"
#include "jeecs/math.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace jeecs::Tilemap
{
    struct MapDocument;
}

namespace jeecs::audio
{
    // "audio_occlusion" 字段内部 schema 版本
    constexpr int32_t AUDIO_OCCLUSION_FORMAT_VERSION = 1;

    // 地图音频遮挡的平整化网格（同一格在所有图层取最大值），运行时与
    // 烘焙共用；按 map_stamp 失效。
    struct audio_map_grid
    {
        uint64_t m_stamp;
        int32_t m_width;
        int32_t m_height;
        std::vector<float> m_occlusion;
        std::vector<float> m_muffle;

        audio_map_grid();
    };

    // 把地图所有图层的音频系数合并成平整化网格（需要图集已解析）。
    void rebuild_audio_grid(
        audio_map_grid& grid, Tilemap::MapDocument& document);

    // 世界空间 2D 遮挡场。
    struct audio_occlusion_field
    {
        // 烘焙时静态输入的哈希（静态遮挡体值 + 地图/图集缓存源 CRC）。
        uint64_t m_inputs;
        float m_cell_size;
        float m_origin_x;
        float m_origin_y;
        int32_t m_width;
        int32_t m_height;
        // uint16 -> 单位长度密度的换算上限；值 = round(density / dmax * 65535)
        float m_density_scale;
        std::vector<uint16_t> m_occlusion;
        std::vector<uint16_t> m_muffle;

        audio_occlusion_field() noexcept;
    };

    struct audio_occlusion_result
    {
        float m_gain;
        float m_hf;

        audio_occlusion_result() noexcept
            : m_gain(1.0f)
            , m_hf(1.0f)
        {
        }
    };

    // 静态输入的跨进程稳定哈希：
    //   * is_static 的 Audio::Occluder + Translation（世界变换解析后的值）；
    //   * 每个 Tilemap::Map + Translation 的路径、世界变换、地图与图集
    //     缓存文件的源 CRC64。
    // 运行时约每秒复核一次，与缓存里的值不一致时回退实时遮挡。
    uint64_t hash_static_occlusion_inputs(je_game_world_t* world);

    // 把静态遮挡烘进 <world_path>.je4cache 的 "audio_occlusion" 字段。
    // world_path 必须已存在于磁盘上（缓存键是它的 CRC64）。
    bool bake_audio_occlusion_world(
        je_game_world_t* world, const char* world_path);

    // 读取 World::Path 组件指向的缓存（传入 <world>.je4cache 路径）。
    std::optional<audio_occlusion_field> load_audio_occlusion_cache(
        const char* cache_path);

    // 2D 射线积分（忽略 z）：沿 listener -> source 积分场密度，
    // gain = exp(-τ_occlusion)，hf = exp(-τ_muffle)。
    audio_occlusion_result integrate_occlusion_field(
        const audio_occlusion_field& field,
        const math::vec3& from,
        const math::vec3& to);
}
