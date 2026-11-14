#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/audio.hpp"
#include "jeecs/components.hpp"
#include "jeecs/game.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace jeecs::audio
{
    // 单张地图的平整化音频遮挡网格：按 map_stamp 失效，只有地图/图集
    // 版本变化时才重建，射线查询只做数组索引。
    struct audio_map_grid
    {
        uint64_t m_stamp;
        int32_t m_width;
        int32_t m_height;
        std::vector<float> m_occlusion;
        std::vector<float> m_muffle;

        audio_map_grid()
            : m_stamp(0)
            , m_width(0)
            , m_height(0)
            , m_occlusion()
            , m_muffle()
        {
        }
    };

    struct AudioUpdatingSystem : game_system
    {
        AudioUpdatingSystem(game_world w);
        void CommitUpdate();

    private:
        // 每个活跃混响区域对应一个 EFX EAX 混响效果 + 效果槽。
        // 槽重建/效果参数变化时 m_slot_epoch 递增，音源据此决定是否重绑发送。
        struct reverb_room_runtime
        {
            bool m_active;
            uint64_t m_slot_epoch;
            basic::optional<basic::resource<effect<je_audio_effect_eax_reverb_t>>> m_effect;
            basic::optional<basic::resource<effect_slot>> m_slot;
            je_audio_effect_eax_reverb_t m_params;
            bool m_has_params;
            float m_slot_gain;

            reverb_room_runtime() noexcept
                : m_active(false)
                , m_slot_epoch(0)
                , m_effect()
                , m_slot()
                , m_params()
                , m_has_params(false)
                , m_slot_gain(-1.0f)
            {
            }
        };

        // 区域实体 id -> 运行时效果槽
        std::unordered_map<uint32_t, reverb_room_runtime> m_reverb_rooms;
        uint64_t m_next_reverb_epoch;

        // 地图路径 -> 音频遮挡网格（按 map_stamp 失效）
        std::unordered_map<std::string, audio_map_grid> m_audio_maps;
    };
}
