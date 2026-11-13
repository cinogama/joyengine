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
        // 听众当前所在的最内层混响区域及其参数；无区域时 m_reverb_active 为 false。
        bool  m_reverb_active;
        float m_reverb_decay_time;
        float m_reverb_gain;
        float m_reverb_gain_hf;
        float m_reverb_diffusion;

        // 区域参数变化时递增；音源按代数决定是否需要重新绑定/解绑第 0 路发送。
        uint64_t m_reverb_generation;

        std::optional<basic::resource<effect<je_audio_effect_reverb_t>>> m_reverb_effect;
        std::optional<basic::resource<effect_slot>>                      m_reverb_slot;

        // 地图路径 -> 音频遮挡网格（按 map_stamp 失效）
        std::unordered_map<std::string, audio_map_grid> m_audio_maps;
    };
}
