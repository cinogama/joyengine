#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif

// ============================================================================
// jeecs_core_audio_environment.hpp —— 组件驱动的音频环境（房间/门洞图）
//
// 房间由 Audio::ReverbZone 表示，门洞由 Audio::Portal 表示。本文件定义
// 烘焙与运行时共用的纯数据结构和图构建/缓存读写接口；不依赖 tilemap。
// ============================================================================

#include "jeecs/audio.hpp"
#include "jeecs/math.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace jeecs::audio
{
    // 室外/无房间节点的房间 ID 哨兵
    constexpr uint32_t AUDIO_OUTDOOR_ROOM_ID = 0xFFFFFFFFu;
    constexpr int32_t AUDIO_ENVIRONMENT_FORMAT_VERSION = 1;

    struct audio_environment_room
    {
        uint32_t m_id;
        math::vec3 m_center;
        math::quat m_rotation;
        math::vec3 m_half_extent;
        float m_fade;
        je_audio_effect_eax_reverb_preset_t m_preset;
        float m_decay_time;
        float m_gain;
        float m_gain_hf;
        float m_diffusion;
        float m_send_gain;

        audio_environment_room() noexcept
            : m_id(0)
            , m_center()
            , m_rotation()
            , m_half_extent()
            , m_fade(0.0f)
            , m_preset(JE_AUDIO_EAX_REVERB_PRESET_CUSTOM)
            , m_decay_time(0.0f)
            , m_gain(0.0f)
            , m_gain_hf(0.0f)
            , m_diffusion(0.0f)
            , m_send_gain(0.0f)
        {
        }
    };

    struct audio_environment_portal
    {
        uint32_t m_room_a; // room id 或 AUDIO_OUTDOOR_ROOM_ID
        uint32_t m_room_b;
        math::vec3 m_center;
        math::vec3 m_normal;
        float m_transmission;
        float m_muffle;

        audio_environment_portal() noexcept
            : m_room_a(AUDIO_OUTDOOR_ROOM_ID)
            , m_room_b(AUDIO_OUTDOOR_ROOM_ID)
            , m_center()
            , m_normal(0.0f, 0.0f, 1.0f)
            , m_transmission(0.0f)
            , m_muffle(0.0f)
        {
        }
    };

    struct audio_environment_path
    {
        float m_transmission; // 路径上所有门洞 transmission 的乘积
        float m_muffle;       // 1 - Π(1 - 门洞 muffle)
        int32_t m_portal_count;

        audio_environment_path() noexcept
            : m_transmission(0.0f)
            , m_muffle(0.0f)
            , m_portal_count(0)
        {
        }
    };

    struct audio_environment
    {
        bool m_use_2d;
        uint64_t m_signature; // 运行时用于判断是否需要重建；不写入缓存
        std::vector<audio_environment_room> m_rooms;
        std::vector<audio_environment_portal> m_portals;
        // 节点数 = 房间数 + 1（室外节点位于 rooms.size()）；
        // 行主序 [from * node_count + to]。
        std::vector<audio_environment_path> m_paths;

        audio_environment() noexcept
            : m_use_2d(false)
            , m_signature(0)
            , m_rooms()
            , m_portals()
            , m_paths()
        {
        }

        size_t node_count() const noexcept
        {
            return m_rooms.size() + 1;
        }
        uint32_t node_of_room(uint32_t room_id) const noexcept
        {
            return room_id == AUDIO_OUTDOOR_ROOM_ID
                ? (uint32_t)m_rooms.size()
                : room_id;
        }
        const audio_environment_path& path(
            uint32_t from_node, uint32_t to_node) const noexcept;
    };

    // 图构建输入：房间/门洞的组件快照，世界变换已解析。
    struct audio_environment_room_snapshot
    {
        uint32_t m_source_id;
        math::vec3 m_center;
        math::quat m_rotation;
        math::vec3 m_half_extent;
        float m_fade;
        je_audio_effect_eax_reverb_preset_t m_preset;
        float m_decay_time;
        float m_gain;
        float m_gain_hf;
        float m_diffusion;
        float m_send_gain;

        audio_environment_room_snapshot() noexcept
            : m_source_id(0)
            , m_center()
            , m_rotation()
            , m_half_extent()
            , m_fade(0.0f)
            , m_preset(JE_AUDIO_EAX_REVERB_PRESET_CUSTOM)
            , m_decay_time(0.0f)
            , m_gain(0.0f)
            , m_gain_hf(0.0f)
            , m_diffusion(0.0f)
            , m_send_gain(0.0f)
        {
        }
    };

    struct audio_environment_portal_snapshot
    {
        uint32_t m_source_id;
        math::vec3 m_center;
        math::vec3 m_normal;
        float m_half_thickness;
        float m_transmission;
        float m_muffle;

        audio_environment_portal_snapshot() noexcept
            : m_source_id(0)
            , m_center()
            , m_normal(0.0f, 0.0f, 1.0f)
            , m_half_thickness(0.0f)
            , m_transmission(0.0f)
            , m_muffle(0.0f)
        {
        }
    };

    // 房间权重：房间内为 1，区域外在 fade 内平滑到 0；use_2d 时忽略 z。
    float audio_environment_room_weight(
        const math::vec3& point,
        const audio_environment_room& room,
        bool use_2d);

    // 返回权重最高的房间 id；没有房间时返回 AUDIO_OUTDOOR_ROOM_ID。
    uint32_t audio_environment_find_room(
        const std::vector<audio_environment_room>& rooms,
        const math::vec3& point,
        bool use_2d);

    // 输入快照哈希；输入未变化时不用重建图。
    uint64_t hash_audio_environment_inputs(
        const std::vector<audio_environment_room_snapshot>& rooms,
        const std::vector<audio_environment_portal_snapshot>& portals,
        bool use_2d);

    // 由组件快照构建房间/门洞图与全房间对路径表。
    audio_environment build_audio_environment(
        const std::vector<audio_environment_room_snapshot>& rooms,
        const std::vector<audio_environment_portal_snapshot>& portals,
        bool use_2d);

    // 写/读 <world_path>.je4cache 的 audio_environment 字段。
    bool write_audio_environment_cache(
        const char* world_path, const audio_environment& environment);
    std::optional<audio_environment> load_audio_environment_cache(
        const char* world_path);

    // 从世界的 Audio::ReverbZone / Audio::Portal 组件烘焙音频环境。
    // 只读取组件与实体变换，不要求世界已启用/已 update，也不要求安装
    // AudioUpdatingSystem。
    bool bake_audio_environment_world(
        je_game_world_t* world, const char* world_path);
}
