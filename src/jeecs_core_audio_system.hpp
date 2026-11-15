#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/audio.hpp"
#include "jeecs/components.hpp"
#include "jeecs/game.hpp"
#include "jeecs_core_audio_environment.hpp"
#include "jeecs_core_audio_occlusion_cache.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace jeecs::audio
{
    struct AudioUpdatingSystem : game_system
    {
        AudioUpdatingSystem(game_world w);
        void CommitUpdate();

        // 请求下一帧重新读取 World::Path 组件指向的遮挡缓存
        void RequestOcclusionCacheReload() noexcept
        {
            m_occlusion_cache_reload_requested = true;
        }

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

        // 环境房间 id -> 运行时效果槽
        std::unordered_map<uint32_t, reverb_room_runtime> m_reverb_rooms;
        uint64_t m_next_reverb_epoch;

        // 房间/门洞图始终由组件实时构建；缓存只存静态遮挡场。
        audio_environment m_live_environment;
        uint64_t m_live_environment_signature;
        bool m_has_live_environment;

        // 诊断日志状态：仅在房间/门洞数量变化时打印一次
        size_t m_logged_environment_rooms;
        size_t m_logged_environment_portals;

        // 世界级 2D 静态遮挡场：由 World::Path 指向的缓存加载；缺失、
        // 输入变化（静态遮挡体/地图被编辑）或 3D 模式时走实时遮挡。
        std::optional<audio_occlusion_field> m_occlusion_field;
        std::string m_loaded_occlusion_cache_path;
        bool m_occlusion_cache_reload_requested;
        // 距离下一次输入哈希复核的剩余时间（秒）
        float m_occlusion_validation_countdown;

        // 地图路径 -> 音频遮挡网格（仅无可用遮挡场时使用，按 map_stamp 失效）
        std::unordered_map<std::string, audio_map_grid> m_audio_maps;
    };
}
