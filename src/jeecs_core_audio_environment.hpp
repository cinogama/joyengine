#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif

// ============================================================================
// jeecs_core_audio_environment.hpp —— 组件驱动的音频环境（房间/门洞图）
//
// 房间由 Audio::ReverbZone 表示，门洞由 Audio::Portal 表示。本文件定义
// 运行时使用的纯数据结构与图构建接口；不依赖 tilemap，也不写缓存。
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

        audio_environment_path() noexcept
            : m_transmission(0.0f)
            , m_muffle(0.0f)
        {
        }
    };

    struct audio_environment
    {
        std::vector<audio_environment_room> m_rooms;
        std::vector<audio_environment_portal> m_portals;
        // 节点数 = 房间数 + 1（室外节点位于 rooms.size()）；
        // 行主序 [from * node_count + to]。
        std::vector<audio_environment_path> m_paths;

        audio_environment() noexcept
            : m_rooms()
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
}
